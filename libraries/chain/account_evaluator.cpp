/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <fc/smart_ref_impl.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/internal_exceptions.hpp>
#include <graphene/chain/special_authority.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <algorithm>

namespace graphene
{
namespace chain
{

void verify_authority_accounts(const database &db, const authority &a)
{
      const auto &chain_params = db.get_global_properties().parameters;
      GRAPHENE_ASSERT(
          a.num_auths() <= chain_params.maximum_authority_membership,
          internal_verify_auth_max_auth_exceeded,
          "Maximum authority membership exceeded");
      for (const auto &acnt : a.account_auths)
      {
            GRAPHENE_ASSERT(db.find_object(acnt.first) != nullptr,
                            internal_verify_auth_account_not_found,
                            "Account ${a} specified in authority does not exist",
                            ("a", acnt.first));
      }
}
template<typename Candidate_Type>
void account_update_evaluator::modify_candidate(Candidate_Type& candidate,vote_id_type vote,const flat_set<vote_id_type>& new_support,const flat_set<vote_id_type>&cancellation_support)
{
      auto temp_itr=new_support.find(vote);
      if ( temp_itr!= new_support.end())
      {
            auto candidate_supporter_itr=candidate.supporters.find(acnt->id);
            if(candidate_supporter_itr!=candidate.supporters.end())     
            {
                  candidate.total_votes+=(vote_lock.amount-candidate_supporter_itr->second.amount).value;
                  candidate_supporter_itr->second=vote_lock;
            }
            else
            {
                  candidate.supporters.insert(make_pair(acnt->id,vote_lock));
                  candidate.total_votes+=vote_lock.amount.value;
            }
      }
      if (cancellation_support.find(vote) != cancellation_support.end())
      {
            auto candidate_supporter_itr=candidate.supporters.find(acnt->id);
            if(candidate_supporter_itr!=candidate.supporters.end())     
            {
                  candidate.supporters.erase(candidate_supporter_itr);
                  candidate.total_votes-=candidate_supporter_itr->second.amount.value;
            }
      }

}


int64_t account_update_evaluator::verify_account_votes(const account_options &options, const flat_set<vote_id_type> &old_votes)
{
      // ensure account's votes satisfy requirements
      // NB only the part of vote checking that requires chain state is here,
      // the rest occurs in account_options::validate()
      auto &d=db();
      flat_set<vote_id_type> new_support;
      flat_set<vote_id_type> cancellation_support;
      flat_set<vote_id_type> affected_voting;
      for (auto &temp_vote : options.votes)
      {
            auto temp_itr=old_votes.find(temp_vote);
            if ( temp_itr== old_votes.end()||this->acnt->asset_locked.vote_locked!=vote_lock)
                  new_support.insert(temp_vote);
            affected_voting.insert(temp_vote);
      }
      for (auto &temp_vote : old_votes)
      {
            if (options.votes.find(temp_vote) == options.votes.end())
                  cancellation_support.insert(temp_vote);
            affected_voting.insert(temp_vote);
      }
      const auto &committee_idx = d.get_index_type<committee_member_index>().indices().get<by_vote_id>();
      const auto &witness_idx = d.get_index_type<witness_index>().indices().get<by_vote_id>();
      for (auto vote : affected_voting) // associate an account with its identity(witness, committee) information,and assert the vote contents
      {
            switch (vote.type())
            {
            case vote_id_type::committee:
            {
                  auto committee_obj = committee_idx.find(vote);
                  FC_ASSERT(committee_obj != committee_idx.end());
                  d.modify(*committee_obj, [&](committee_member_object &com) {
                        modify_candidate(com,vote,new_support,cancellation_support);
                  });
                  break;
            }
            case vote_id_type::witness:
            {
                  auto witness_obj = witness_idx.find(vote);
                  FC_ASSERT(witness_obj != witness_idx.end());
                  d.modify(*witness_obj, [&](witness_object &wit) {
                         modify_candidate(wit,vote,new_support,cancellation_support);
                  });
                  break;
            }
            default:
                  FC_THROW("Invalid Vote Type: ${id}", ("id", vote));
                  break;
            }
      }
      // }
}

void_result account_create_evaluator::do_evaluate(const account_create_operation &op)
{
      try
      {
            database &d = db();
            FC_ASSERT(fee_paying_account->is_lifetime_member(), "Only Lifetime members may register an account.");

            try
            {
                  verify_authority_accounts(d, op.owner);
                  verify_authority_accounts(d, op.active);
            }
            GRAPHENE_RECODE_EXC(internal_verify_auth_max_auth_exceeded, account_create_max_auth_exceeded)
            GRAPHENE_RECODE_EXC(internal_verify_auth_account_not_found, account_create_auth_account_not_found)

            if (op.extensions.value.owner_special_authority.valid())
                  evaluate_special_authority(d, *op.extensions.value.owner_special_authority);
            if (op.extensions.value.active_special_authority.valid())
                  evaluate_special_authority(d, *op.extensions.value.active_special_authority);

            auto &acnt_indx = d.get_index_type<account_index>();
            if (op.name.size())
            {
                  auto current_account_itr = acnt_indx.indices().get<by_name>().find(op.name);
                  FC_ASSERT(current_account_itr == acnt_indx.indices().get<by_name>().end());
            }

            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((op))
}

object_id_result account_create_evaluator::do_apply(const account_create_operation &o)
{
      try
      {

            database &d = db();
            const auto &new_acnt_object = db().create<account_object>([&](account_object &obj) {
                  obj.registrar = o.registrar;
                  obj.name = o.name;
                  obj.owner = o.owner;
                  obj.active = o.active;
                  obj.options = o.options;
                  obj.statistics = db().create<account_statistics_object>([&](account_statistics_object &s) { s.owner = obj.id; }).id;

                  if (o.extensions.value.owner_special_authority.valid())
                        obj.owner_special_authority = o.extensions.value.owner_special_authority;
                  if (o.extensions.value.active_special_authority.valid())
                        obj.active_special_authority = o.extensions.value.active_special_authority;
                  
            });
            const auto &dynamic_properties = db().get_dynamic_global_properties();
            db().modify(dynamic_properties, [](dynamic_global_property_object &p) {
                  ++p.accounts_registered_this_interval;
            });

            const auto &global_properties = db().get_global_properties();
            if (dynamic_properties.accounts_registered_this_interval % global_properties.parameters.accounts_per_fee_scale == 0)
                  db().modify(global_properties, [&dynamic_properties](global_property_object &p) {
                        p.parameters.current_fees->get<account_create_operation>().basic_fee <<= p.parameters.account_fee_scale_bitshifts;
                  });

            if (o.extensions.value.owner_special_authority.valid() || o.extensions.value.active_special_authority.valid())
            {
                  db().create<special_authority_object>([&](special_authority_object &sa) {
                        sa.account = new_acnt_object.id;
                  });
            }
            return new_acnt_object.id;
      }
      FC_CAPTURE_AND_RETHROW((o))
}

void_result account_update_evaluator::do_evaluate(const account_update_operation &o)
{
      try
      {
            database &d = db();
            try
            {
                  if (o.owner)
                        verify_authority_accounts(d, *o.owner);
                  if (o.active)
                        verify_authority_accounts(d, *o.active);
            }
            GRAPHENE_RECODE_EXC(internal_verify_auth_max_auth_exceeded, account_update_max_auth_exceeded)
            GRAPHENE_RECODE_EXC(internal_verify_auth_account_not_found, account_update_auth_account_not_found)
            if(o.new_options)
            {
                  if(o.new_options->votes.size()==0)
                        FC_ASSERT(o.lock_with_vote.asset_id==asset_id_type()&&o.lock_with_vote.amount==0);
                  auto  num_witness=0, num_committee=0;
                  for( vote_id_type id : o.new_options->votes )
                  {
                  if( id.type() == vote_id_type::witness )
                        num_witness++;
                  else if ( id.type() == vote_id_type::committee)
                       num_committee++;
                  }
                  auto chain_params=d.get_global_properties().parameters;
                  FC_ASSERT(num_witness <= chain_params.witness_number_of_election,"Voted for more witnesses than currently allowed (${c})", ("c", chain_params.witness_number_of_election));
                  FC_ASSERT(num_committee <= chain_params.committee_number_of_election,"Voted for more committee members than currently allowed (${c})", ("c", chain_params.committee_number_of_election));
            }
            if (o.extensions.value.owner_special_authority.valid())
                  evaluate_special_authority(d, *o.extensions.value.owner_special_authority);
            if (o.extensions.value.active_special_authority.valid())
                  evaluate_special_authority(d, *o.extensions.value.active_special_authority);

            acnt = &o.account(d);
                  

            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((o))
}

void_result account_update_evaluator::do_apply(const account_update_operation &o)
{
      try
      {
            database &d = db();
            bool sa_before, sa_after;
            d.modify(*acnt, [&](account_object &a) {
                  if (o.owner)
                  {
                        a.owner = *o.owner;
                        a.top_n_control_flags = 0;
                  }
                  if (o.active)
                  {
                        a.active = *o.active;
                        a.top_n_control_flags = 0;
                  }
                  if (o.new_options)
                  {
                        vote_lock=o.lock_with_vote;
                        FC_ASSERT(vote_lock.asset_id==asset_id_type());
                        verify_account_votes(*o.new_options, acnt->options.votes);
                        if(a.asset_locked.vote_locked.valid())
                        {     
                              a.asset_locked.locked_total[a.asset_locked.vote_locked->asset_id]+=vote_lock.amount-a.asset_locked.vote_locked->amount;
                              a.asset_locked.vote_locked=vote_lock;
                        }
                        else{
                              a.asset_locked.vote_locked=vote_lock;
                              a.asset_locked.locked_total[a.asset_locked.vote_locked->asset_id]+=a.asset_locked.vote_locked->amount;
                        }
                        
                        a.options = *o.new_options;
                  }
                  sa_before = a.has_special_authority();
                  if (o.extensions.value.owner_special_authority.valid())
                  {
                        a.owner_special_authority = *(o.extensions.value.owner_special_authority);
                        a.top_n_control_flags = 0;
                  }
                  if (o.extensions.value.active_special_authority.valid())
                  {
                        a.active_special_authority = *(o.extensions.value.active_special_authority);
                        a.top_n_control_flags = 0;
                  }
                  sa_after = a.has_special_authority();
            });

            if (sa_before && (!sa_after))
            {
                  const auto &sa_idx = d.get_index_type<special_authority_index>().indices().get<by_account>();
                  auto sa_it = sa_idx.find(o.account);
                  assert(sa_it != sa_idx.end());
                  d.remove(*sa_it);
            }
            else if ((!sa_before) && sa_after)
            {
                  d.create<special_authority_object>([&](special_authority_object &sa) {
                        sa.account = o.account;
                  });
            }

            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((o))
}
/*
void_result account_whitelist_evaluator::do_evaluate(const account_whitelist_operation &o)
{
      try
      {
            database &d = db();

            listed_account = &o.account_to_list(d);
            if (!d.get_global_properties().parameters.allow_non_member_whitelists)
                  FC_ASSERT(o.authorizing_account(d).is_lifetime_member());

            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((o))
}

void_result account_whitelist_evaluator::do_apply(const account_whitelist_operation &o)
{
      try
      {
            database &d = db();

            d.modify(*listed_account, [&o](account_object &a) {
                  if (o.new_listing & o.white_listed)
                        a.whitelisting_accounts.insert(o.authorizing_account);
                  else
                        a.whitelisting_accounts.erase(o.authorizing_account);

                  if (o.new_listing & o.black_listed)
                        a.blacklisting_accounts.insert(o.authorizing_account);
                  else
                        a.blacklisting_accounts.erase(o.authorizing_account);
            });
      
            // for tracking purposes only, this state is not needed to evaluate /
            d.modify(o.authorizing_account(d), [&](account_object &a) {
                  if (o.new_listing & o.white_listed)
                        a.whitelisted_accounts.insert(o.account_to_list);
                  else
                        a.whitelisted_accounts.erase(o.account_to_list);

                  if (o.new_listing & o.black_listed)
                        a.blacklisted_accounts.insert(o.account_to_list);
                  else
                        a.blacklisted_accounts.erase(o.account_to_list);
            });

            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((o))
}
*/
void_result account_upgrade_evaluator::do_evaluate(const account_upgrade_evaluator::operation_type &o)
{
      try
      {
            database &d = db();

            account = &d.get(o.account_to_upgrade);
            FC_ASSERT(!account->is_lifetime_member());

            return {};
            //} FC_CAPTURE_AND_RETHROW( (o) ) }
      }
      FC_RETHROW_EXCEPTIONS(error, "Unable to upgrade account '${a}'", ("a", o.account_to_upgrade(db()).name))
}

void_result account_upgrade_evaluator::do_apply(const account_upgrade_evaluator::operation_type &o)
{
      try
      {
            database &d = db();

            d.modify(*account, [&](account_object &a) {
                  if (o.upgrade_to_lifetime_member)
                  {
                        a.membership_expiration_date = time_point_sec::maximum();
                  }
            });
            return {};
      }
      FC_RETHROW_EXCEPTIONS(error, "Unable to upgrade account '${a}'", ("a", o.account_to_upgrade(db()).name))
}

} // namespace chain
} // namespace graphene
