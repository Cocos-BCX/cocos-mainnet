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
#include <graphene/chain/protocol/base.hpp>

namespace graphene
{
namespace chain
{

/**
    * op_wrapper is used to get around the circular definition of operation and proposals that contain them.
    */
struct op_wrapper;

// create timed task
struct crontab_create_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
      uint32_t price_per_kbyte = 10;
   };
   account_id_type crontab_creator;  // crontab's creator
   vector<op_wrapper> crontab_ops;   // the operations that the task will do
   time_point_sec start_time;        // the task's start time
   uint64_t execute_interval;        // execution interval
   uint64_t scheduled_execute_times; // task scheduled execution times
   extensions_type extensions;

   account_id_type fee_payer() const { return crontab_creator; }
   void validate() const;
   share_type calculate_fee(const fee_parameters_type &k) const;
};

struct crontab_cancel_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
   };
   account_id_type fee_paying_account;
   crontab_id_type task;
   extensions_type extensions;

   account_id_type fee_payer() const { return fee_paying_account; }
   void validate() const;
};

// recover suspended crontab
struct crontab_recover_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
   };
   account_id_type crontab_owner;
   crontab_id_type crontab;
   time_point_sec restart_time; // time when the task restarts
   extensions_type extensions;

   account_id_type fee_payer() const { return crontab_owner; }
   void validate() const;
};
///@}

} // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::crontab_create_operation::fee_parameters_type, (fee)(price_per_kbyte))
FC_REFLECT(graphene::chain::crontab_cancel_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::crontab_recover_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::crontab_create_operation, (crontab_creator)(crontab_ops)(start_time)(execute_interval)(scheduled_execute_times)(extensions))

FC_REFLECT(graphene::chain::crontab_cancel_operation, (fee_paying_account)(task)(extensions))
FC_REFLECT(graphene::chain::crontab_recover_operation, (crontab_owner)(crontab)(restart_time)(extensions))
