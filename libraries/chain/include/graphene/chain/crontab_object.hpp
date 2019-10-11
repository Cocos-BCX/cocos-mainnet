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

#include <graphene/chain/protocol/transaction.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <graphene/db/generic_index.hpp>

namespace graphene
{
namespace chain
{

/**
 *  @brief tracks the crontab of a partially timed transaction 
 *  @ingroup object
 *  @ingroup protocol
 */
class crontab_object : public abstract_object<crontab_object>
{
    public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = crontab_object_type;

      account_id_type task_owner; // the task's creator
      transaction timed_transaction;   // task execution content
      uint64_t execute_interval;       // execution interval
      uint64_t scheduled_execute_times;   // task scheduled execution times
      uint64_t already_execute_times;         // the number of times the task has been executed
      time_point_sec last_execte_time; // last execution time
      time_point_sec next_execte_time; // next execution time
      tx_hash_type trx_hash;
      uint32_t continuous_failure_times = 0;
      bool is_suspended = false; // If the task execution fails consecutively 3 times, it will be suspended
      bool allow_execution=false;
      time_point_sec expiration_time;

      bool is_authorized_to_execute(database &db) const;
};

struct by_task_expiration
{
};
struct by_task_owner
{
};
struct by_susupend_status_and_next_execute_time
{
};


typedef boost::multi_index_container<
    crontab_object,
    indexed_by<
        ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
        ordered_non_unique<tag<by_task_owner>,
                           composite_key<crontab_object,
                                         member<crontab_object, account_id_type, &crontab_object::task_owner>,
                                         member<crontab_object, bool, &crontab_object::is_suspended>,
                                         member<crontab_object, time_point_sec, &crontab_object::next_execte_time>>>,
        ordered_non_unique<tag<by_susupend_status_and_next_execute_time>,
                           composite_key<crontab_object,
                                         member<crontab_object, bool, &crontab_object::is_suspended>,
                                         member<crontab_object, time_point_sec, &crontab_object::next_execte_time>>>,
        ordered_non_unique<tag<by_task_expiration>, member<crontab_object, time_point_sec, &crontab_object::expiration_time>>>>
    crontab_multi_index_container;
typedef generic_index<crontab_object, crontab_multi_index_container> crontab_index;

} // namespace chain
} // namespace graphene

FC_REFLECT_DERIVED(graphene::chain::crontab_object, (graphene::chain::object),
                   (task_owner)(timed_transaction)(execute_interval)(scheduled_execute_times)(already_execute_times)(last_execte_time)
                   (next_execte_time)(trx_hash)(continuous_failure_times)(is_suspended)(allow_execution)(expiration_time))
