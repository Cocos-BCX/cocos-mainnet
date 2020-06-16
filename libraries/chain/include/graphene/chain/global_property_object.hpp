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
#pragma once
#include <fc/uint128.hpp>

#include <graphene/chain/protocol/chain_parameters.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/db/object.hpp>

namespace graphene { namespace chain {


   class unsuccessful_candidates_object: public graphene::db::abstract_object<unsuccessful_candidates_object>
   {
      public:
         static const uint8_t space_id = extension_id_for_nico;
         static const uint8_t type_id  = unsuccessful_candidates_type;
         flat_set<account_id_type> unsuccessful_candidates;
   };

   /**
    * @class global_property_object
    * @brief Maintains global state information (committee_member list, current fees)
    * @ingroup object
    * @ingroup implementation
    *
    * This is an implementation detail. The values here are set by committee_members to tune the blockchain parameters.
    */
   class global_property_object : public graphene::db::abstract_object<global_property_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_global_property_object_type;

         chain_parameters           parameters;
         optional<chain_parameters> pending_parameters;

         uint32_t                           next_available_vote_id = 0;
         vector<committee_member_id_type>   active_committee_members; // updated once per maintenance interval
         flat_set<witness_id_type>          active_witnesses; // updated once per maintenance interval
         // n.b. witness scheduling is done by witness_schedule object
   };

   /**
    * @class dynamic_global_property_object
    * @brief Maintains global state information (committee_member list, current fees)
    * @ingroup object
    * @ingroup implementation
    *
    * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
    * current values of global blockchain properties.
    */
   class dynamic_global_property_object : public abstract_object<dynamic_global_property_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_dynamic_global_property_object_type;

         uint32_t          head_block_number = 0; //NICO 当前区块高度
         block_id_type     head_block_id;         //当前最高区块区块id
         time_point_sec    time;                  //当前最高区块时间
         witness_id_type   current_witness;       //当前最高区块出块人
         uint32_t          current_transaction_count=0;//当前区块包含交易数量
         time_point_sec    next_maintenance_time; //下一次链维护时间，链维护将更新投票统计，更新出块人以及理事会
         time_point_sec    last_budget_time;      //下一次预算分配时间，出块奖励分配等等
         share_type        witness_budget;        //本周期内出块人奖励总预算剩余量
         uint32_t          accounts_registered_this_interval = 0;//当前维护周期内注册账户总数，同一周期内过多的账户注册，会提高注册账户op的基础手续费
         /**
          *  Every time a block is missed this increases by
          *  RECENTLY_MISSED_COUNT_INCREMENT,
          *  every time a block is found it decreases by
          *  RECENTLY_MISSED_COUNT_DECREMENT.  It is
          *  never less than 0.
          *
          *  If the recently_missed_count hits 2*UNDO_HISTORY then no new blocks may be pushed.
          */
         uint32_t          recently_missed_count = 0;  //最近遗漏的区块数量统计

         /**
          * The current absolute slot number.  Equal to the total
          * number of slots since genesis.  Also equal to the total
          * number of missed slots plus head_block_number.
          */
         uint64_t                current_aslot = 0;   //当前区块槽位

         /**
          * used to compute witness participation.
          */
         fc::uint128_t recent_slots_filled;          //当前槽位填充推算

         /**
          * dynamic_flags specifies chain state properties that can be
          * expressed in one bit.
          */
         uint32_t dynamic_flags = 0;                //状态标识位：描述当前链是否到了维护期等等

         uint32_t last_irreversible_block_num = 0;  //当前链最后不可逆的区块高度

         enum dynamic_flag_bits
         {
            /**
             * If maintenance_flag is set, then the head block is a
             * maintenance block.  This means
             * get_time_slot(1) - head_block_time() will have a gap
             * due to maintenance duration.
             *
             * This flag answers the question, "Was maintenance
             * performed in the last call to apply_block()?"
             */
            maintenance_flag = 0x01
         };
   };
}}

FC_REFLECT_DERIVED( graphene::chain::dynamic_global_property_object, (graphene::db::object),
                    (head_block_number)
                    (head_block_id)
                    (time)
                    (current_witness)
                    (current_transaction_count)
                    (next_maintenance_time)
                    (last_budget_time)
                    (witness_budget)
                    (accounts_registered_this_interval)
                    (recently_missed_count)
                    (current_aslot)
                    (recent_slots_filled)
                    (dynamic_flags)
                    (last_irreversible_block_num)
                  )

FC_REFLECT_DERIVED( graphene::chain::global_property_object, (graphene::db::object),
                    (parameters)
                    (pending_parameters)
                    (next_available_vote_id)
                    (active_committee_members)
                    (active_witnesses)
                  )
FC_REFLECT_DERIVED( graphene::chain::unsuccessful_candidates_object, (graphene::db::object),(unsuccessful_candidates))
                  
