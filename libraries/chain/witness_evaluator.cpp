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
#include <graphene/chain/witness_evaluator.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/vote.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/chain_property_object.hpp>

namespace graphene
{
namespace chain
{

void_result witness_create_evaluator::do_evaluate(const witness_create_operation &op)
{
      try
      {
            auto &_db = db();
            FC_ASSERT(_db.get(op.witness_account).is_lifetime_member());
            FC_ASSERT(_db.get_balance(op.witness_account, asset_id_type()).amount.value >= _db.get_global_properties().parameters.witness_candidate_freeze);
            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((op))
}

object_id_result witness_create_evaluator::do_apply(const witness_create_operation &op)
{
      try
      {
            vote_id_type vote_id;
            auto &_db = db();
            auto candidate_freeze = _db.get_global_properties().parameters.witness_candidate_freeze;
            _db.modify(_db.get_global_properties(), [&vote_id](global_property_object &p) {
                  vote_id = get_next_vote_id(p, vote_id_type::witness);
            });

            const auto &new_witness_object = _db.create<witness_object>([&](witness_object &obj) {
                  obj.witness_account = op.witness_account;
                  obj.signing_key = op.block_signing_key;
                  obj.vote_id = vote_id;
                  obj.url = op.url;
                  obj.work_status = true;
                  obj.next_maintenance_time = _db.get_dynamic_global_properties().next_maintenance_time;
                  obj.total_votes+=candidate_freeze.value;
            });
            
            _db.modify(new_witness_object.witness_account(_db), [&](account_object &witness) {
                  witness.witness_status = std::make_pair(new_witness_object.id, true);
                  witness.asset_locked.witness_freeze = candidate_freeze;
                  witness.asset_locked.locked_total[asset_id_type()] += candidate_freeze;
                  FC_ASSERT(*witness.asset_locked.witness_freeze >= asset());
                  _db.assert_balance(witness,asset(candidate_freeze));  
            });
            return new_witness_object.id;
      }
      FC_CAPTURE_AND_RETHROW((op))
}

void_result witness_update_evaluator::do_evaluate(const witness_update_operation &op)
{
      try
      {
            auto &d = db();
            FC_ASSERT(d.get(op.witness).witness_account == op.witness_account);
            if (!op.work_status)
            {
                  auto on_the_job = d.get_index_type<witness_index>().indices().get<by_work_status>().equal_range(true);
                  FC_ASSERT(d.get_chain_properties().immutable_parameters.min_witness_count < boost::distance(on_the_job));
            }
            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((op))
}

void_result witness_update_evaluator::do_apply(const witness_update_operation &op)
{
      try
      {
            database &_db = db();
            const auto &witness = _db.get(op.witness);
            auto candidate_freeze = _db.get_global_properties().parameters.witness_candidate_freeze;
            auto next_maintenance_time = _db.get_dynamic_global_properties().next_maintenance_time;
            auto &witness_account = op.witness_account(_db);
            FC_ASSERT(witness.next_maintenance_time != next_maintenance_time);
            bool change_status = witness.work_status != op.work_status;
            _db.modify(witness, [&](witness_object &wit) {
                  if (change_status)
                  {
                        wit.work_status = op.work_status;
                        wit.next_maintenance_time = next_maintenance_time;
                        wit.work_status ? wit.total_votes += candidate_freeze.value :
                        wit.total_votes -= witness_account.asset_locked.witness_freeze->amount.value;
                  }
                  if (op.new_url.valid())
                        wit.url = *op.new_url;
                  if (op.new_signing_key.valid())
                        wit.signing_key = *op.new_signing_key;
            });
            if (change_status)
                  _db.modify(witness_account, [&](account_object &account) {
                        bool status = op.work_status;
                        account.witness_status->second = op.work_status;
                        if (!status)
                        {
                              if (account.asset_locked.witness_freeze.valid())
                              {
                                    FC_ASSERT(*account.asset_locked.witness_freeze >= asset() &&
                                              account.asset_locked.locked_total[asset_id_type()] >= account.asset_locked.witness_freeze->amount);
                                    account.asset_locked.locked_total[asset_id_type()] -= account.asset_locked.witness_freeze->amount;
                                    account.asset_locked.witness_freeze = {};
                              }
                        }
                        else
                        {
                              if (!account.asset_locked.witness_freeze.valid())
                              {
                                    account.asset_locked.witness_freeze = candidate_freeze;
                                    FC_ASSERT(*account.asset_locked.witness_freeze >= asset());
                                    account.asset_locked.locked_total[asset_id_type()] += candidate_freeze;
                              }
                        }
                  });
            _db.assert_balance(witness_account,asset(candidate_freeze)); 
            return void_result();
      }
      FC_CAPTURE_AND_RETHROW((op))
}

} // namespace chain
} // namespace graphene
