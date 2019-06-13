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

#include <graphene/chain/vesting_balance_object.hpp>

namespace graphene { namespace chain {

inline bool sum_below_max_shares(const asset& a, const asset& b)
{
   assert(GRAPHENE_MAX_SHARE_SUPPLY + GRAPHENE_MAX_SHARE_SUPPLY > GRAPHENE_MAX_SHARE_SUPPLY);
   return (a.amount              <= GRAPHENE_MAX_SHARE_SUPPLY)
       && (            b.amount  <= GRAPHENE_MAX_SHARE_SUPPLY)
       && ((a.amount + b.amount) <= GRAPHENE_MAX_SHARE_SUPPLY);
}

asset linear_vesting_policy::get_allowed_withdraw( const vesting_policy_context& ctx )const
{
    share_type allowed_withdraw = 0;

    if( ctx.now > begin_timestamp )
    {
        const auto elapsed_seconds = (ctx.now - begin_timestamp).to_seconds();
        assert( elapsed_seconds > 0 );

        if( elapsed_seconds >= vesting_cliff_seconds )
        {
            share_type total_vested = 0;
            if( elapsed_seconds < vesting_duration_seconds )
            {
                total_vested = (fc::uint128_t( begin_balance.value ) * elapsed_seconds / vesting_duration_seconds).to_uint64();
            }
            else
            {
                total_vested = begin_balance;
            }
            assert( total_vested >= 0 );

            const share_type withdrawn_already = begin_balance - ctx.balance.amount;
            assert( withdrawn_already >= 0 );

            allowed_withdraw = total_vested - withdrawn_already;
            assert( allowed_withdraw >= 0 );
        }
    }

    return asset( allowed_withdraw, ctx.balance.asset_id );
}

void linear_vesting_policy::on_deposit(const vesting_policy_context& ctx)
{
}

bool linear_vesting_policy::is_deposit_allowed(const vesting_policy_context& ctx)const
{
   return (ctx.amount.asset_id == ctx.balance.asset_id)
      && sum_below_max_shares(ctx.amount, ctx.balance);
}

void linear_vesting_policy::on_withdraw(const vesting_policy_context& ctx)
{
}

bool linear_vesting_policy::is_withdraw_allowed(const vesting_policy_context& ctx)const
{
   return (ctx.amount.asset_id == ctx.balance.asset_id)
          && (ctx.amount <= get_allowed_withdraw(ctx));
}

fc::uint128_t cdd_vesting_policy::compute_coin_seconds_earned(const vesting_policy_context& ctx)const
{
   assert(ctx.now >= coin_seconds_earned_last_update);
   int64_t delta_seconds = (ctx.now - coin_seconds_earned_last_update).to_seconds();
   assert(delta_seconds >= 0);

   fc::uint128_t delta_coin_seconds = ctx.balance.amount.value;
   delta_coin_seconds *= delta_seconds;

   fc::uint128_t coin_seconds_earned_cap = ctx.balance.amount.value;
   coin_seconds_earned_cap *= std::max(vesting_seconds, 1u);

   return std::min(coin_seconds_earned + delta_coin_seconds, coin_seconds_earned_cap);
}

void cdd_vesting_policy::update_coin_seconds_earned(const vesting_policy_context& ctx)
{
   coin_seconds_earned = compute_coin_seconds_earned(ctx);
   coin_seconds_earned_last_update = ctx.now;
}

asset cdd_vesting_policy::get_allowed_withdraw(const vesting_policy_context& ctx)const
{
   if(ctx.now <= start_claim)
      return asset(0, ctx.balance.asset_id);
   fc::uint128_t cs_earned = compute_coin_seconds_earned(ctx);
   fc::uint128_t withdraw_available = cs_earned / std::max(vesting_seconds, 1u);
   assert(withdraw_available <= ctx.balance.amount.value);
   return asset(withdraw_available.to_uint64(), ctx.balance.asset_id);
}

void cdd_vesting_policy::on_deposit(const vesting_policy_context& ctx)
{
   update_coin_seconds_earned(ctx);
}

void cdd_vesting_policy::on_deposit_vested(const vesting_policy_context& ctx)
{
   on_deposit(ctx);
   coin_seconds_earned += ctx.amount.amount.value * std::max(vesting_seconds, 1u);
}

void cdd_vesting_policy::on_withdraw(const vesting_policy_context& ctx)
{
   update_coin_seconds_earned(ctx);
   fc::uint128_t coin_seconds_needed = ctx.amount.amount.value;
   coin_seconds_needed *= std::max(vesting_seconds, 1u);
   // is_withdraw_allowed should forbid any withdrawal that
   // would trigger this assert
   assert(coin_seconds_needed <= coin_seconds_earned);

   coin_seconds_earned -= coin_seconds_needed;
}

bool cdd_vesting_policy::is_deposit_allowed(const vesting_policy_context& ctx)const
{
   return (ctx.amount.asset_id == ctx.balance.asset_id)
         && sum_below_max_shares(ctx.amount, ctx.balance);
}

bool cdd_vesting_policy::is_deposit_vested_allowed(const vesting_policy_context& ctx) const
{
   return is_deposit_allowed(ctx);
}

bool cdd_vesting_policy::is_withdraw_allowed(const vesting_policy_context& ctx)const
{
   return (ctx.amount <= get_allowed_withdraw(ctx));
}

#define VESTING_VISITOR(NAME, MAYBE_CONST)                    \
struct NAME ## _visitor                                       \
{                                                             \
   typedef decltype(                                          \
      std::declval<linear_vesting_policy>().NAME(             \
         std::declval<vesting_policy_context>())              \
     ) result_type;                                           \
                                                              \
   NAME ## _visitor(                                          \
      const asset& balance,                                   \
      const time_point_sec& now,                              \
      const asset& amount                                     \
     )                                                        \
   : ctx(balance, now, amount) {}                             \
                                                              \
   template< typename Policy >                                \
   result_type                                                \
   operator()(MAYBE_CONST Policy& policy) MAYBE_CONST         \
   {                                                          \
      return policy.NAME(ctx);                                \
   }                                                          \
                                                              \
   vesting_policy_context ctx;                                \
}

VESTING_VISITOR(on_deposit,);
VESTING_VISITOR(on_deposit_vested,);
VESTING_VISITOR(on_withdraw,);
VESTING_VISITOR(is_deposit_allowed, const);
VESTING_VISITOR(is_deposit_vested_allowed, const);
VESTING_VISITOR(is_withdraw_allowed, const);
VESTING_VISITOR(get_allowed_withdraw, const);

bool vesting_balance_object::is_deposit_allowed(const time_point_sec& now, const asset& amount)const
{
   return policy.visit(is_deposit_allowed_visitor(balance, now, amount));
}

bool vesting_balance_object::is_withdraw_allowed(const time_point_sec& now, const asset& amount)const
{
   bool result = policy.visit(is_withdraw_allowed_visitor(balance, now, amount));
   // if some policy allows you to withdraw more than your balance,
   //   there's a programming bug in the policy algorithm
   assert((amount <= balance) || (!result));
   return result;
}

void vesting_balance_object::deposit(const time_point_sec& now, const asset& amount)
{
   on_deposit_visitor vtor(balance, now, amount);
   policy.visit(vtor);
   balance += amount;
}

void vesting_balance_object::deposit_vested(const time_point_sec& now, const asset& amount)
{
   on_deposit_vested_visitor vtor(balance, now, amount);
   policy.visit(vtor);
   balance += amount;
}

bool vesting_balance_object::is_deposit_vested_allowed(const time_point_sec& now, const asset& amount) const
{
   return policy.visit(is_deposit_vested_allowed_visitor(balance, now, amount));
}

void vesting_balance_object::withdraw(const time_point_sec& now, const asset& amount)
{
   assert(amount <= balance);
   on_withdraw_visitor vtor(balance, now, amount);
   policy.visit(vtor);
   balance -= amount;
}

asset vesting_balance_object::get_allowed_withdraw(const time_point_sec& now)const
{
   asset amount = asset();
   return policy.visit(get_allowed_withdraw_visitor(balance, now, amount));
}

} } // graphene::chain
