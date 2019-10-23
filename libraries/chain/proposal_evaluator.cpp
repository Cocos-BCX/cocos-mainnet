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
#include <graphene/chain/asset_evaluator.hpp>
#include <graphene/chain/proposal_evaluator.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/world_view_object.hpp>
#include <graphene/chain/nh_asset_creator_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <fc/smart_ref_impl.hpp>
#include <graphene/chain/db_notify.hpp>

namespace graphene
{
namespace chain
{

void_result proposal_create_evaluator::do_evaluate(const proposal_create_operation &o)
{
    try
    {
        const database &d = db();
        const auto &global_parameters = d.get_global_properties().parameters;

        FC_ASSERT(o.expiration_time > d.head_block_time(), "Proposal has already expired on creation.");
        FC_ASSERT(o.expiration_time <= d.head_block_time() + global_parameters.maximum_proposal_lifetime,
                  "Proposal expiration time is too far in the future.");
        FC_ASSERT(!o.review_period_seconds || fc::seconds(*o.review_period_seconds) < (o.expiration_time - d.head_block_time()),
                  "Proposal review period must be less than its overall lifetime.");

        {
            // If we're dealing with the committee authority, make sure this transaction has a sufficient review period.
            flat_set<account_id_type> auths;
            vector<authority> other;
            for (auto &op : o.proposed_ops)
            {
                operation_get_required_authorities(op.op, auths, auths, other);
            }

            FC_ASSERT(other.size() == 0,"required other authorities:${other}",("other",other)); // TODO: what about other???

            if (auths.find(GRAPHENE_COMMITTEE_ACCOUNT) != auths.end()||auths.find(GRAPHENE_WITNESS_ACCOUNT)!=auths.end())
            {
                GRAPHENE_ASSERT(
                    o.review_period_seconds.valid(),
                    proposal_create_review_period_required,
                    "Review period not given, but at least ${min} required,proposal:${proposal}",
                    ("min", global_parameters.committee_proposal_review_period)("proposal", o));
                GRAPHENE_ASSERT(
                    *o.review_period_seconds >= global_parameters.committee_proposal_review_period,
                    proposal_create_review_period_insufficient,
                    "Review period of ${t} specified, but at least ${min} required",
                    ("t", *o.review_period_seconds)("min", global_parameters.committee_proposal_review_period));
                FC_ASSERT(o.expiration_time<=d.get_dynamic_global_properties().next_maintenance_time,
                    "The proposal must end before the arrival of a new term in the group account");   
            }
            
            // Verify that the impacted account exists
            for (auto &op : o.proposed_ops)
            {
                graphene::chain::operation_get_impacted_accounts( op.op, auths );
            }
            for( const account_id_type &id : auths )
            FC_ASSERT(nullptr != d.find(id), "can't find account:${account}",("account",id));
        }

        //relate_world_view_operation
        for (auto &op : o.proposed_ops)
        {
            if (op.op.which() == operation::tag<relate_world_view_operation>::value)
            {
                const relate_world_view_operation &pr_op = op.op.get<relate_world_view_operation>();
                auto &nh_asset_creator_idx = d.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();
                //Verify that if the proposer is a nht creator
                const auto &popposer_idx = nh_asset_creator_idx.find(o.fee_paying_account);
                FC_ASSERT(nh_asset_creator_idx.find(o.fee_paying_account) != nh_asset_creator_idx.end(), "You are not a nh asset creater, so you can't create a world view.");

                //Verify that if the world view exists
                const auto &version_idx_by_name = d.get_index_type<world_view_index>().indices().get<by_world_view>();
                const auto &version_idx = version_idx_by_name.find(pr_op.world_view);
                FC_ASSERT(version_idx != version_idx_by_name.end(), "${version} is not exist.", ("version", pr_op.world_view.c_str()));
                //Verify that if the worldview creator is correct
                const auto &owner_idx = nh_asset_creator_idx.find(pr_op.view_owner);
                FC_ASSERT(version_idx->world_view_creator == owner_idx->id, "${owner} is not the world view's creator.", ("owner", pr_op.view_owner));

                //Verify that the proposer is already related to the world view
                FC_ASSERT(version_idx->related_nht_creator.end() == find(version_idx->related_nht_creator.begin(), version_idx->related_nht_creator.end(), popposer_idx->id),
                          "${popposer} has relate to the world view.", ("popposer", o.fee_paying_account));
            }
        }

        for (const op_wrapper &op : o.proposed_ops)
            _proposed_trx.operations.push_back(op.op);
        _proposed_trx.validate();
        /*
   if( d.head_block_time() <= HARDFORK_CORE_429_TIME )
   { // TODO: remove after HARDFORK_CORE_429_TIME has passed
      graphene::chain::impl::hf_429_visitor hf_429;
      hf_429( o );
   }
*/
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

object_id_result proposal_create_evaluator::do_apply(const proposal_create_operation &o)
{
    try
    {
        database &d = db();
        auto next_id = d.get_index_type<proposal_index>().get_next_id();
        const proposal_object &proposal = d.create<proposal_object>([&](proposal_object &proposal) {
            _proposed_trx.expiration = o.expiration_time;
            _proposed_trx.extensions=vector<string>{string(next_id)};
            proposal.proposed_transaction = _proposed_trx;
            proposal.expiration_time = o.expiration_time;
            proposal.trx_hash = trx_state->_trx->hash();
            if (o.review_period_seconds)
                proposal.review_period_time = o.expiration_time - *o.review_period_seconds;

            //Populate the required approval sets
            flat_set<account_id_type> required_active;
            vector<authority> other;

            // TODO: consider caching values from evaluate?
            for (auto &op : _proposed_trx.operations)
                operation_get_required_authorities(op, required_active, proposal.required_owner_approvals, other);

            //All accounts which must provide both owner and active authority should be omitted from the active authority set;
            //owner authority approval implies active authority approval.
            std::set_difference(required_active.begin(), required_active.end(),
                                proposal.required_owner_approvals.begin(), proposal.required_owner_approvals.end(),
                                std::inserter(proposal.required_active_approvals, proposal.required_active_approvals.begin()));
        });
        FC_ASSERT(proposal.id==next_id);
        return proposal.id;
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result proposal_update_evaluator::do_evaluate(const proposal_update_operation &o)
{
    try
    {
        database &d = db();

        _proposal = &o.proposal(d);

        if (_proposal->review_period_time && d.head_block_time() >= *_proposal->review_period_time)
            FC_ASSERT(o.active_approvals_to_add.empty() && o.owner_approvals_to_add.empty(),
                      "This proposal is in its review period. No new approvals may be added.");

        for (account_id_type id : o.active_approvals_to_remove)
        {
            FC_ASSERT(_proposal->available_active_approvals.find(id) != _proposal->available_active_approvals.end(),
                      "", ("id", id)("available", _proposal->available_active_approvals));
        }
        for (account_id_type id : o.owner_approvals_to_remove)
        {
            FC_ASSERT(_proposal->available_owner_approvals.find(id) != _proposal->available_owner_approvals.end(),
                      "", ("id", id)("available", _proposal->available_owner_approvals));
        }

        /*  All authority checks happen outside of evaluators
   if( (d.get_node_properties().skip_flags & database::skip_authority_check) == 0 )
   {
      for( const auto& id : o.key_approvals_to_add )
      {
         FC_ASSERT( trx_state->signed_by(id) );
      }
      for( const auto& id : o.key_approvals_to_remove )
      {
         FC_ASSERT( trx_state->signed_by(id) );
      }
   }
   */

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result proposal_update_evaluator::do_apply(const proposal_update_operation &o)
{
    try
    {
        database &d = db();
        // Potential optimization: if _executed_proposal is true, we can skip the modify step and make push_proposal skip
        // signature checks. This isn't done now because I just wrote all the proposals code, and I'm not yet 100% sure the
        // required approvals are sufficient to authorize the transaction.
        proposal_object temp = *_proposal;
        temp.available_active_approvals.insert(o.active_approvals_to_add.begin(), o.active_approvals_to_add.end());
        temp.available_owner_approvals.insert(o.owner_approvals_to_add.begin(), o.owner_approvals_to_add.end());

        for (account_id_type id : o.active_approvals_to_remove)
            temp.available_active_approvals.erase(id);

        for (account_id_type id : o.owner_approvals_to_remove)
            temp.available_owner_approvals.erase(id);

        for (const auto &id : o.key_approvals_to_add)
            temp.available_key_approvals.insert(id);

        for (const auto &id : o.key_approvals_to_remove)
            temp.available_key_approvals.erase(id);

        if (temp.is_authorized_to_execute(d))
        {
            if (!temp.review_period_time)
                temp.expiration_time = d.head_block_time();
            temp.proposed_transaction.set_reference_block(d.get_dynamic_global_properties().head_block_id);
            temp.proposed_transaction.set_expiration(temp.expiration_time +std::min<uint32_t>(d.get_global_properties().parameters.assigned_task_life_cycle,7200));
            temp.proposed_transaction.validate();
            temp.allow_execution = true;
        }
        else temp.allow_execution = false;
        d.modify(*_proposal, [&temp](proposal_object &p) { p = temp; });
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
} // namespace chain

void_result proposal_delete_evaluator::do_evaluate(const proposal_delete_operation &o)
{
    try
    {
        database &d = db();

        _proposal = &o.proposal(d);

        auto required_approvals = o.using_owner_authority ? &_proposal->required_owner_approvals
                                                          : &_proposal->required_active_approvals;
        FC_ASSERT(required_approvals->find(o.fee_paying_account) != required_approvals->end(),
                  "Provided authority is not authoritative for this proposal.",
                  ("provided", o.fee_paying_account)("required", *required_approvals));

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result proposal_delete_evaluator::do_apply(const proposal_delete_operation &o)
{
    try
    {
        db().remove(*_proposal);

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

} // namespace chain
} // namespace graphene
