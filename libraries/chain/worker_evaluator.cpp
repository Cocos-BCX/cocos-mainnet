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
{ try {
   database& d = db();

   FC_ASSERT(d.get(o.owner).is_lifetime_member());
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
       w.balance = db.create<vesting_balance_object>([&](vesting_balance_object& b) {
         b.owner = worker.worker_account;
         b.balance = asset(0);

         cdd_vesting_policy policy;
         policy.vesting_seconds = fc::days(i.pay_vesting_period_days).to_seconds();
         policy.coin_seconds_earned = 0;
         policy.coin_seconds_earned_last_update = db.head_block_time();
         b.policy = policy;
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
   vote_id_type for_id, against_id;
   d.modify(d.get_global_properties(), [&for_id, &against_id](global_property_object& p) {
      for_id = get_next_vote_id(p, vote_id_type::worker);
      against_id = get_next_vote_id(p, vote_id_type::worker);
   });

   return d.create<worker_object>([&](worker_object& w) {
      w.worker_account = o.owner;
      w.daily_pay = o.daily_pay;
      w.work_begin_date = o.work_begin_date;
      w.work_end_date = o.work_end_date;
      w.name = o.name;
      w.url = o.url;
      w.vote_for = for_id;
      w.vote_against = against_id;

      w.worker.set_which(o.initializer.which());
      o.initializer.visit( worker_init_visitor( w, d ) );
   }).id;
} FC_CAPTURE_AND_RETHROW( (o) ) }

void refund_worker_type::pay_worker(share_type pay, database& db)
{
   total_burned += pay;
   db.modify(db.get(asset_id_type()).dynamic_data(db), [pay](asset_dynamic_data_object& d) {
      d.current_supply -= pay;
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

} } // graphene::chain
