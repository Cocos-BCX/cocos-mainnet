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

namespace graphene { namespace chain { 

   struct linear_vesting_policy_initializer
   {
      /** while vesting begins on begin_timestamp, none may be claimed before vesting_cliff_seconds have passed */
      fc::time_point_sec begin_timestamp;
      uint32_t           vesting_cliff_seconds = 0;
      uint32_t           vesting_duration_seconds = 0;
   };

   struct cdd_vesting_policy_initializer
   {
      /** while coindays may accrue over time, none may be claimed before the start_claim time */
      fc::time_point_sec start_claim;
      uint32_t           vesting_seconds = 0;
      cdd_vesting_policy_initializer( uint32_t vest_sec = 0, fc::time_point_sec sc = fc::time_point_sec() ):start_claim(sc),vesting_seconds(vest_sec){}
   };

   typedef fc::static_variant<linear_vesting_policy_initializer, cdd_vesting_policy_initializer> vesting_policy_initializer;



   /**
    * @brief Create a vesting balance.
    * @ingroup operations
    *
    *  The chain allows a user to create a vesting balance.
    *  Normally, vesting balances are created automatically as part
    *  of cashback and worker operations.  This operation allows
    *  vesting balances to be created manually as well.
    *
    *  Manual creation of vesting balances can be used by a stakeholder
    *  to publicly demonstrate that they are committed to the chain.
    *  It can also be used as a building block to create transactions
    *  that function like public debt.  Finally, it is useful for
    *  testing vesting balance functionality.
    *
    * @return ID of newly created vesting_balance_object
    */
   struct vesting_balance_create_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };

      account_id_type             creator; ///< Who provides funds initially
      account_id_type             owner; ///< Who is able to withdraw the balance
      asset                       amount;
      vesting_policy_initializer  policy;

      account_id_type   fee_payer()const { return creator; }
      void              validate()const
      {
         FC_ASSERT( amount.amount > share_type(0) );
      }
   };

   /**
    * @brief Withdraw from a vesting balance.
    * @ingroup operations
    *
    * Withdrawal from a not-completely-mature vesting balance
    * will result in paying fees.
    *
    * @return Nothing
    */
   struct vesting_balance_withdraw_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 20*GRAPHENE_BLOCKCHAIN_PRECISION; };
      vesting_balance_id_type vesting_balance;
      account_id_type         owner; ///< Must be vesting_balance.owner
      asset                   amount;

      account_id_type   fee_payer()const { return owner; }
      void              validate()const
      {
         FC_ASSERT( amount.amount > share_type(0) );
      }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::vesting_balance_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::vesting_balance_withdraw_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::vesting_balance_create_operation, (creator)(owner)(amount)(policy) )
FC_REFLECT( graphene::chain::vesting_balance_withdraw_operation, (vesting_balance)(owner)(amount) )

FC_REFLECT(graphene::chain::linear_vesting_policy_initializer, (begin_timestamp)(vesting_cliff_seconds)(vesting_duration_seconds) )
FC_REFLECT(graphene::chain::cdd_vesting_policy_initializer, (start_claim)(vesting_seconds) )
FC_REFLECT_TYPENAME( graphene::chain::vesting_policy_initializer )
