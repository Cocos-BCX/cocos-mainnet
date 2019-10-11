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
#include <graphene/chain/database.hpp>
#include <graphene/chain/worker_evaluator.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <graphene/chain/protocol/vote.hpp>

namespace graphene { namespace chain {

void_result worker_create_evaluator::do_evaluate(const worker_create_evaluator::operation_type& o)
{ 
   try 
   {
      database& d = db();
      FC_ASSERT(o.work_begin_date >= d.head_block_time());
      return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }


struct worker_init_visitor
{
   typedef void result_type;

   worker_object& worker;
   database&      db;

   worker_init_visitor( worker_object& w, database& d ):worker(w),db(d){}

   result_type operator()( const vesting_balance_worker_initializer& i )const
   {
      vesting_balance_worker_type w;
       w.balance = db.create<vesting_balance_object>([&](vesting_balance_object& b) 
       {
         FC_ASSERT(worker.beneficiary.valid());
         b.owner = *worker.beneficiary;
         b.balance = asset(0);

         cdd_vesting_policy policy;
         policy.vesting_seconds = fc::days(i.pay_vesting_period_days).to_seconds();
         policy.coin_seconds_earned = 0;
         policy.coin_seconds_earned_last_update = db.head_block_time();
         b.policy = policy;
         b.describe=i.describe;
      }).id;
      worker.worker = w;
   }

   template<typename T>
   result_type operator()( const T& )const
   {
      // DO NOTHING FOR OTHER WORKERS
   }
};





object_id_result worker_create_evaluator::do_apply(const worker_create_evaluator::operation_type& o)
{ try {
   database& d = db();
   

   return d.create<worker_object>([&](worker_object& w) {
      w.beneficiary = o.beneficiary;
      w.daily_pay = o.daily_pay;
      w.work_begin_date = o.work_begin_date;
      w.work_end_date = o.work_end_date;
      w.name = o.name;
      w.describe = o.describe;

      w.worker.set_which(o.initializer.which());
      o.initializer.visit( worker_init_visitor( w, d ) );
   }).id;
} FC_CAPTURE_AND_RETHROW( (o) ) }

void destroy_worker_type::pay_worker(share_type pay, database& db)
{
   total_burned += pay;
   db.modify(asset_id_type()(db),[&](asset_object&asset_ob)
      {
         FC_ASSERT(asset_ob.reserved(db) >= pay);
         asset_ob.options.max_supply-=pay;
         asset_ob.options.max_market_fee=asset_ob.options.max_market_fee<=asset_ob.options.max_supply?asset_ob.options.max_market_fee:asset_ob.options.max_supply;
      });
}

void vesting_balance_worker_type::pay_worker(share_type pay, database& db)
{
   db.modify(balance(db), [&](vesting_balance_object& b) {
      b.deposit(db.head_block_time(), asset(pay));
   });
}


void burn_worker_type::pay_worker(share_type pay, database& db)
{
   total_burned += pay;
   db.adjust_balance( GRAPHENE_NULL_ACCOUNT, pay );
}

void issuance_worker_type::pay_worker(share_type pay, database&db)
   {
      total_issuance+=pay;
      db.modify(asset_id_type()(db),[&](asset_object&asset_ob)
      {
         asset_ob.options.max_supply+=pay;
      });
   }


} } // graphene::chain
