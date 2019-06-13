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

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/vesting_balance_evaluator.hpp>
#include <graphene/chain/vesting_balance_object.hpp>

namespace graphene { namespace chain {

void_result vesting_balance_create_evaluator::do_evaluate( const vesting_balance_create_operation& op )
{ try {
   const database& d = db();

   const account_object& creator_account = op.creator( d );
   /* const account_object& owner_account = */ op.owner( d );

   // TODO: Check asset authorizations and withdrawals

   FC_ASSERT( op.amount.amount > 0 );
   FC_ASSERT( d.get_balance( creator_account.id, op.amount.asset_id ) >= op.amount );
   FC_ASSERT( !op.amount.asset_id(d).is_transfer_restricted() );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

struct init_policy_visitor
{
   typedef void result_type;

   init_policy_visitor( vesting_policy& po,
                        const share_type& begin_balance,
                        const fc::time_point_sec& n ):p(po),init_balance(begin_balance),now(n){}

   vesting_policy&    p;
   share_type         init_balance;
   fc::time_point_sec now;

   void operator()( const linear_vesting_policy_initializer& i )const
   {
      linear_vesting_policy policy;
      policy.begin_timestamp = i.begin_timestamp;
      policy.vesting_cliff_seconds = i.vesting_cliff_seconds;
      policy.vesting_duration_seconds = i.vesting_duration_seconds;
      policy.begin_balance = init_balance;
      p = policy;
   }

   void operator()( const cdd_vesting_policy_initializer& i )const
   {
      cdd_vesting_policy policy;
      policy.vesting_seconds = i.vesting_seconds;
      policy.start_claim = i.start_claim;
      policy.coin_seconds_earned = 0;
      policy.coin_seconds_earned_last_update = now;
      p = policy;
   }
};

object_id_result vesting_balance_create_evaluator::do_apply( const vesting_balance_create_operation& op )
{ try {
   database& d = db();
   const time_point_sec now = d.head_block_time();

   FC_ASSERT( d.get_balance( op.creator, op.amount.asset_id ) >= op.amount );
   d.adjust_balance( op.creator, -op.amount );  //nico 减少当前拥有量

   const vesting_balance_object& vbo = d.create< vesting_balance_object >( [&]( vesting_balance_object& obj )//nico 创建锁定的保留金
   {
      //WARNING: The logic to create a vesting balance object is replicated in vesting_balance_worker_type::initializer::init.
      // If making changes to this logic, check if those changes should also be made there as well.
      obj.owner = op.owner;
      obj.balance = op.amount;
      op.policy.visit( init_policy_visitor( obj.policy, op.amount.amount, now ) ); //nico 将op中的返现方式以变体访问的形式赋值给obj
   } );


   return vbo.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result vesting_balance_withdraw_evaluator::do_evaluate( const vesting_balance_withdraw_operation& op )
{ try {
   const database& d = db();
   const time_point_sec now = d.head_block_time();

   const vesting_balance_object& vbo = op.vesting_balance( d );
   FC_ASSERT( op.owner == vbo.owner, "", ("op.owner", op.owner)("vbo.owner", vbo.owner) );
   FC_ASSERT( vbo.is_withdraw_allowed( now, op.amount ), "", ("now", now)("op", op)("vbo", vbo) );
   assert( op.amount <= vbo.balance );      // is_withdraw_allowed should fail before this check is reached

   /* const account_object& owner_account = */ op.owner( d );
   // TODO: Check asset authorizations and withdrawals
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result vesting_balance_withdraw_evaluator::do_apply( const vesting_balance_withdraw_operation& op )
{ try {
   database& d = db();
   const time_point_sec now = d.head_block_time();

   const vesting_balance_object& vbo = op.vesting_balance( d );

   // Allow zero balance objects to stick around, (1) to comply
   // with the chain's "objects live forever" design principle, (2)
   // if it's cashback or worker, it'll be filled up again.

   d.modify( vbo, [&]( vesting_balance_object& vbo )
   {
      vbo.withdraw( now, op.amount );
   } );

   d.adjust_balance( op.owner, op.amount );

   // TODO: Check asset authorizations and withdrawals
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
