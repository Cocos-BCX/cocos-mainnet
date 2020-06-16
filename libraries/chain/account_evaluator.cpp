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
template <typename Candidate_Type>
void account_update_evaluator::modify_candidate(Candidate_Type &candidate, const flat_set<vote_id_type> &new_support, const flat_set<vote_id_type> &changed_support, const flat_set<vote_id_type> &cancellation_support)
{
      if (new_support.find(candidate.vote_id) != new_support.end())
      {
            if (is_concerned)
                  candidate.supporters.insert(make_pair(acnt->id, vote_lock));
            candidate.total_votes += vote_lock.amount.value;
      }
      if (changed_support.find(candidate.vote_id) != changed_support.end())
      {
            if (is_concerned)
            {
                  auto candidate_supporter_itr = candidate.supporters.find(acnt->id);
                  if (candidate_supporter_itr != candidate.supporters.end())
                        candidate_supporter_itr->second = vote_lock;
                  else
                        candidate.supporters.insert(make_pair(acnt->id, vote_lock));
            }
            candidate.total_votes += temp.value;
      }
      if (cancellation_support.find(candidate.vote_id) != cancellation_support.end())
      {
            if (is_concerned)
            {
                  auto candidate_supporter_itr = candidate.supporters.find(acnt->id);
                  if (candidate_supporter_itr != candidate.supporters.end())
                        candidate.supporters.erase(candidate_supporter_itr);
            }
            candidate.total_votes -= old_vote_lock.amount.value;
      }
}

int64_t account_update_evaluator::verify_account_votes(const account_options &options, const flat_set<vote_id_type> &old_votes)
{
      // ensure account's votes satisfy requirements
      // NB only the part of vote checking that requires chain state is here,
      // the rest occurs in account_options::validate()
      auto &d = db();
      flat_set<vote_id_type> new_support;
      flat_set<vote_id_type> changed_support;
      flat_set<vote_id_type> cancellation_support;
      flat_set<vote_id_type> affected_voting;
      final_vote = options.votes;
      for (auto &temp_vote : options.votes)
      {
            auto temp_itr = old_votes.find(temp_vote);
            if (temp_itr == old_votes.end())
                  new_support.insert(temp_vote);
            else if (temp.value)
                  changed_support.insert(temp_vote);
            affected_voting.insert(temp_vote);
      }
      for (auto &temp_vote : old_votes)
      {
            if (temp_vote.type() == _vote_type)
            {
                  if (options.votes.find(temp_vote) == options.votes.end())
                        cancellation_support.insert(temp_vote);
                  affected_voting.insert(temp_vote);
            }
            else
                  final_vote.insert(temp_vote);
      }
      const auto &committee_idx = d.get_index_type<committee_member_index>().indices().get<by_vote_id>();
      const auto &witness_idx = d.get_index_type<witness_index>().indices().get<by_vote_id>();
      for (auto vote : affected_voting) // associate an account with its identity(witness, committee) information,and assert the vote contents
      {
            is_concerned = false;
            if(d.concerned_candidates.size()>0)
            {
                  auto content = d.concerned_candidates.begin()->content;
          
                  if (content == 0)
                      is_concerned = true;  
            }

            if (d.concerned_candidates.find(vote) != d.concerned_candidates.end())
                  is_concerned = true;
            switch (vote.type())
            {
            case vote_id_type::committee:
            {
                  auto committee_obj = committee_idx.find(vote);
                  FC_ASSERT(committee_obj != committee_idx.end());
                  d.modify(*committee_obj, [&](committee_member_object &com) {
                        modify_candidate(com, new_support, changed_support, cancellation_support);
                  });
                  break;
            }
            case vote_id_type::witness:
            {
                  auto witness_obj = witness_idx.find(vote);
                  FC_ASSERT(witness_obj != witness_idx.end());
                  d.modify(*witness_obj, [&](witness_object &wit) {
                        modify_candidate(wit, new_support, changed_support, cancellation_support);
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
            const auto &new_acnt_object = d.create<account_object>([&](account_object &obj) {
                  obj.registrar = o.registrar;
                  obj.name = o.name;
                  obj.owner = o.owner;
                  obj.active = o.active;
                  obj.options = o.options;
                  obj.statistics = d.create<account_statistics_object>([&](account_statistics_object &s) { s.owner = obj.id; }).id;

                  if (o.extensions.value.owner_special_authority.valid())
                        obj.owner_special_authority = o.extensions.value.owner_special_authority;
                  if (o.extensions.value.active_special_authority.valid())
                        obj.active_special_authority = o.extensions.value.active_special_authority;
            });
            const auto &dynamic_properties = d.get_dynamic_global_properties();
            d.modify(dynamic_properties, [](dynamic_global_property_object &p) {
                  ++p.accounts_registered_this_interval;
            });

            const auto &global_properties = d.get_global_properties();
            if (dynamic_properties.accounts_registered_this_interval % global_properties.parameters.accounts_per_fee_scale == 0)
                  d.modify(global_properties, [&dynamic_properties](global_property_object &p) {
                        p.parameters.current_fees->get<account_create_operation>().basic_fee <<= p.parameters.account_fee_scale_bitshifts;
                  });

            if (o.extensions.value.owner_special_authority.valid() || o.extensions.value.active_special_authority.valid())
            {
                  d.create<special_authority_object>([&](special_authority_object &sa) {
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
            if (o.new_options)
            {
                  if (o.lock_with_vote)
                  {
                        bool vote_amount = o.lock_with_vote->second.amount.value ? true : false;
                        bool vote_size = o.new_options->votes.size() ? true : false;
                        FC_ASSERT(o.lock_with_vote->second.asset_id == asset_id_type() && (vote_amount ^ vote_size == 0));
                        FC_ASSERT(o.lock_with_vote->first <= vote_id_type::vote_type::vote_noone);
                        _vote_type = vote_id_type::vote_type(o.lock_with_vote->first);
                        for (vote_id_type id : o.new_options->votes)
                        {
                              if (id.type() == vote_id_type::witness)
                                    num_witness++;
                              else if (id.type() == vote_id_type::committee)
                                    num_committee++;
                        }
                        if (num_committee.value)
                              FC_ASSERT(_vote_type == vote_id_type::vote_type::committee);
                        if (num_witness.value)
                              FC_ASSERT(_vote_type == vote_id_type::vote_type::witness);
                        auto chain_params = d.get_global_properties().parameters;
                        FC_ASSERT(num_witness <= chain_params.witness_number_of_election, "Voted for more witnesses than currently allowed (${c})", ("c", chain_params.witness_number_of_election));
                        FC_ASSERT(num_committee <= chain_params.committee_number_of_election, "Voted for more committee members than currently allowed (${c})", ("c", chain_params.committee_number_of_election));
                  }
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
                        if (o.lock_with_vote)
                        {
                              vote_lock = o.lock_with_vote->second;
                              FC_ASSERT(vote_lock.asset_id == asset_id_type() && vote_lock.amount >= 0);
                              if (o.lock_with_vote->first != vote_id_type::vote_type::vote_noone)
                              {
                                    auto &real_vote_locked = o.lock_with_vote->first == vote_id_type::vote_type::witness ? a.asset_locked.vote_for_witness : a.asset_locked.vote_for_committee;
                                    if (real_vote_locked.valid())
                                    {
                                          old_vote_lock = *real_vote_locked;
                                          temp = vote_lock.amount - real_vote_locked->amount;
                                          a.asset_locked.locked_total[real_vote_locked->asset_id] += temp.value;
                                          if (temp.value < 0)
                                          {
                                                optional<vesting_balance_id_type> new_vbid = d.deposit_lazy_vesting(
                                                    acnt->cashback_vote, -temp,
                                                    d.get_global_properties().parameters.cashback_vote_period_seconds,
                                                    acnt->id, "cashback_vote", true);
                                                if (new_vbid.valid())
                                                      a.cashback_vote = new_vbid;
                                                d.adjust_balance(a.id, temp);
                                          }
                                    }
                                    else
                                          a.asset_locked.locked_total[vote_lock.asset_id] += vote_lock.amount;
                                    verify_account_votes(*o.new_options, acnt->options.votes);
                                    if (vote_lock.amount == 0)
                                          real_vote_locked = {};
                                    else
                                          real_vote_locked = vote_lock;
                              }
                              d.assert_balance(a,vote_lock);
                        }
                        a.options = *o.new_options;
                        if (o.lock_with_vote)
                              a.options.votes = final_vote;
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
