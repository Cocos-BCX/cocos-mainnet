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
#include <graphene/chain/committee_member_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/protocol/vote.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/committee_member_object.hpp>

#include <fc/smart_ref_impl.hpp>

namespace graphene
{
namespace chain
{

void_result committee_member_create_evaluator::do_evaluate(const committee_member_create_operation &op)
{
   try
   {
      auto &_db = db();
      FC_ASSERT(_db.get(op.committee_member_account).is_lifetime_member());
      FC_ASSERT(_db.get_balance(op.committee_member_account, asset_id_type()).amount.value >= _db.get_global_properties().parameters.committee_candidate_freeze);
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

object_id_result committee_member_create_evaluator::do_apply(const committee_member_create_operation &op)
{
   try
   {
      vote_id_type vote_id;
      auto &_db = db();
      _db.modify(_db.get_global_properties(), [&vote_id](global_property_object &p) {
         vote_id = get_next_vote_id(p, vote_id_type::committee);
      });
      auto candidate_freeze = _db.get_global_properties().parameters.committee_candidate_freeze;
      const auto &new_del_object = _db.create<committee_member_object>([&](committee_member_object &obj) {
         obj.committee_member_account = op.committee_member_account;
         obj.vote_id = vote_id;
         obj.url = op.url;
         obj.work_status = true;
         obj.next_maintenance_time = _db.get_dynamic_global_properties().next_maintenance_time;
         obj.total_votes += candidate_freeze.value;
      });

      _db.modify(new_del_object.committee_member_account(_db), [&](account_object &committee) {
         committee.committee_status = std::make_pair(new_del_object.id, true);
         committee.asset_locked.committee_freeze = candidate_freeze;
         committee.asset_locked.locked_total[asset_id_type()] += candidate_freeze;
         FC_ASSERT(*committee.asset_locked.committee_freeze >= asset(0));
         _db.assert_balance(committee,*committee.asset_locked.committee_freeze);   
      });
      
      return new_del_object.id;
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void_result committee_member_update_evaluator::do_evaluate(const committee_member_update_operation &op)
{
   try
   {
      auto &_db = db();
      FC_ASSERT(_db.get(op.committee_member).committee_member_account == op.committee_member_account);
      FC_ASSERT(_db.get_balance(op.committee_member_account, asset_id_type()).amount.value >= _db.get_global_properties().parameters.committee_candidate_freeze);
      if (!op.work_status)
      {
         auto on_the_job = _db.get_index_type<committee_member_index>().indices().get<by_work_status>().equal_range(true);
         FC_ASSERT(_db.get_chain_properties().immutable_parameters.min_committee_member_count < boost::distance(on_the_job));
      }
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void_result committee_member_update_evaluator::do_apply(const committee_member_update_operation &op)
{
   try
   {

      database &_db = db();
      const auto &commitee = _db.get(op.committee_member);
      auto &committee_account = op.committee_member_account(_db);
      auto candidate_freeze = _db.get_global_properties().parameters.committee_candidate_freeze;
      auto next_maintenance_time = _db.get_dynamic_global_properties().next_maintenance_time;
      FC_ASSERT(commitee.next_maintenance_time != next_maintenance_time);
      bool change_status = commitee.work_status != op.work_status;
      _db.modify(commitee, [&](committee_member_object &com) {
         if (change_status)
         {
            com.work_status = op.work_status;
            com.next_maintenance_time = next_maintenance_time;
            com.work_status ? com.total_votes += candidate_freeze.value : 
            com.total_votes -= committee_account.asset_locked.committee_freeze->amount.value;
         }
         if (op.new_url.valid())
            com.url = *op.new_url;
      });
      if (change_status)
         _db.modify(committee_account, [&](account_object &account) {
            bool status = op.work_status;
            account.committee_status->second = op.work_status;
            if (!status)
            {
               if (account.asset_locked.committee_freeze.valid())
               {
                  FC_ASSERT(*account.asset_locked.committee_freeze >= asset() &&
                            account.asset_locked.locked_total[asset_id_type()] >= account.asset_locked.committee_freeze->amount);
                  account.asset_locked.locked_total[asset_id_type()] -= account.asset_locked.committee_freeze->amount;
                  account.asset_locked.committee_freeze = {};
               }
            }
            else
            {
               if (!account.asset_locked.committee_freeze.valid())
               {

                  account.asset_locked.committee_freeze = candidate_freeze;
                  FC_ASSERT(*account.asset_locked.committee_freeze >= asset());
                  account.asset_locked.locked_total[asset_id_type()] += candidate_freeze;
               }
            }
         });
       _db.assert_balance(committee_account,asset(candidate_freeze));  
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void_result committee_member_update_global_parameters_evaluator::do_evaluate(const committee_member_update_global_parameters_operation &o)
{
   try
   {
      FC_ASSERT(trx_state->is_agreed_task);

      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((o))
}

void_result committee_member_update_global_parameters_evaluator::do_apply(const committee_member_update_global_parameters_operation &o)
{
   try
   {
      auto &_db=db();
      auto &gp =_db.get_global_properties();
      auto&cp=_db.get_chain_properties();
      // 禁止理事会修改系统维护周期
      FC_ASSERT(o.new_parameters.maintenance_interval == gp.parameters.maintenance_interval, "modification of system maintenance interval isn`t allowed");
      FC_ASSERT(o.new_parameters.witness_number_of_election>=cp.immutable_parameters.min_witness_count&&
                o.new_parameters.witness_number_of_election<=_db.get_index_type<witness_index>().indices().size());
      
      FC_ASSERT(o.new_parameters.committee_number_of_election>=cp.immutable_parameters.min_committee_member_count&&
                o.new_parameters.committee_number_of_election<=_db.get_index_type<committee_member_index>().indices().size());
      
      _db.modify(gp, [&o](global_property_object &p) {
         p.pending_parameters = o.new_parameters;
      });

      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((o))
}

} // namespace chain
} // namespace graphene
