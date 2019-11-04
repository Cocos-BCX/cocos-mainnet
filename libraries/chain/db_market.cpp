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

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/market_object.hpp>

#include <fc/uint128.hpp>

namespace graphene
{
namespace chain
{

/**
 * All margin positions are force closed at the swan price
 * Collateral received goes into a force-settlement fund
 * No new margin positions can be created for this asset
 * Force settlement happens without delay at the swan price, deducting from force-settlement fund
 * No more asset updates may be issued.
*/
void database::globally_settle_asset(const asset_object &mia, const price &settlement_price)
{
   try
   {
      /*
   elog( "BLACK SWAN!" );
   debug_dump();
   edump( (mia.symbol)(settlement_price) );
   */

      const asset_bitasset_data_object &bitasset = mia.bitasset_data(*this);
      FC_ASSERT(!bitasset.has_settlement(), "black swan already occurred, it should not happen again");

      const asset_object &backing_asset = bitasset.options.short_backing_asset(*this);
      asset collateral_gathered = backing_asset.amount(0);

      const asset_dynamic_data_object &mia_dyn = mia.dynamic_asset_data_id(*this);
      auto original_mia_supply = mia_dyn.current_supply;

      const call_order_index &call_index = get_index_type<call_order_index>();
      const auto &call_price_index = call_index.indices().get<by_price>();

      // cancel all call orders and accumulate it into collateral_gathered
      auto call_itr = call_price_index.lower_bound(price::min(bitasset.options.short_backing_asset, mia.id));
      auto call_end = call_price_index.upper_bound(price::max(bitasset.options.short_backing_asset, mia.id));
      while (call_itr != call_end)
      {
         auto pays = call_itr->get_debt() * settlement_price;

         if (pays > call_itr->get_collateral())
            pays = call_itr->get_collateral();

         collateral_gathered += pays;
         const auto &order = *call_itr;
         ++call_itr;
         FC_ASSERT(fill_order(order, pays, order.get_debt(), settlement_price, true)); // call order is maker
      }

      modify(bitasset, [&](asset_bitasset_data_object &obj) {
         assert(collateral_gathered.asset_id == settlement_price.quote.asset_id);
         obj.settlement_price = mia.amount(original_mia_supply) / collateral_gathered; //settlement_price;
         obj.settlement_fund = collateral_gathered.amount;
      });

      /// After all margin positions are closed, the current supply will be reported as 0, but
      /// that is a lie, the supply didn't change.   We need to capture the current supply before
      /// filling all call orders and then restore it afterward.   Then in the force settlement
      /// evaluator reduce the supply
      modify(mia_dyn, [&](asset_dynamic_data_object &obj) {
         obj.current_supply = original_mia_supply;
      });
   }
   FC_CAPTURE_AND_RETHROW((mia)(settlement_price))
}
//nico log::在清算之后重新开启智能资产
void database::revive_bitasset(const asset_object &bitasset)
{
   try
   {
      FC_ASSERT(bitasset.is_market_issued());
      const asset_bitasset_data_object &bad = bitasset.bitasset_data(*this);
      FC_ASSERT(bad.has_settlement());
      const asset_dynamic_data_object &bdd = bitasset.dynamic_asset_data_id(*this);
      //FC_ASSERT(!bad.is_prediction_market); 取消二元预测市场
      FC_ASSERT(!bad.current_feed.settlement_price.is_null());

      if (bdd.current_supply > 0)
      {
         // Create + execute a "bid" with 0 additional collateral
         const collateral_bid_object &pseudo_bid = create<collateral_bid_object>([&](collateral_bid_object &bid) {
            bid.bidder = bitasset.issuer;
            bid.inv_swan_price = asset(0, bad.options.short_backing_asset) / asset(bdd.current_supply, bitasset.id);
         });
         execute_bid(pseudo_bid, bdd.current_supply, bad.settlement_fund, bad.current_feed);
      }
      else
         FC_ASSERT(bad.settlement_fund == 0);

      _cancel_bids_and_revive_mpa(bitasset, bad);
   }
   FC_CAPTURE_AND_RETHROW((bitasset))
}

void database::_cancel_bids_and_revive_mpa(const asset_object &bitasset, const asset_bitasset_data_object &bad)
{
   try
   {
      FC_ASSERT(bitasset.is_market_issued());
      FC_ASSERT(bad.has_settlement());
      //FC_ASSERT(!bad.is_prediction_market);  取消二元预测市场

      // cancel remaining bids
      const auto &bid_idx = get_index_type<collateral_bid_index>().indices().get<by_price>();
      auto itr = bid_idx.lower_bound(boost::make_tuple(bitasset.id, price::max(bad.options.short_backing_asset, bitasset.id), collateral_bid_id_type()));
      while (itr != bid_idx.end() && itr->inv_swan_price.quote.asset_id == bitasset.id)
      {
         const collateral_bid_object &bid = *itr;
         ++itr;
         cancel_bid(bid);
      }

      // revive
      modify(bad, [&](asset_bitasset_data_object &obj) {
         obj.settlement_price = price();
         obj.settlement_fund = 0;
      });
   }
   FC_CAPTURE_AND_RETHROW((bitasset))
}

void database::cancel_bid(const collateral_bid_object &bid, bool create_virtual_op)
{
   adjust_balance(bid.bidder, bid.inv_swan_price.base);

   if (create_virtual_op)
   {
      bid_collateral_operation vop;
      vop.bidder = bid.bidder;
      vop.additional_collateral = bid.inv_swan_price.base;
      vop.debt_covered = asset(0, bid.inv_swan_price.quote.asset_id);
      auto op_id = push_applied_operation(vop);
      set_applied_operation_result(op_id, object_id_result(operation_history_id_type(op_id)));
   }
   remove(bid);
}

void database::execute_bid(const collateral_bid_object &bid, share_type debt_covered, share_type collateral_from_fund, const price_feed &current_feed)
{
   const call_order_object &call_obj = create<call_order_object>([&](call_order_object &call) {
      call.borrower = bid.bidder;
      call.collateral = bid.inv_swan_price.base.amount + collateral_from_fund;
      call.debt = debt_covered;
      call.call_price = price::call_price(asset(debt_covered, bid.inv_swan_price.quote.asset_id),
                                          asset(call.collateral, bid.inv_swan_price.base.asset_id),
                                          current_feed.maintenance_collateral_ratio);
   });

   if (bid.inv_swan_price.base.asset_id == asset_id_type())
      modify(bid.bidder(*this).statistics(*this), [&](account_statistics_object &stats) {
         stats.total_core_in_orders += call_obj.collateral;
      });

   auto op_id=push_applied_operation(execute_bid_operation(bid.bidder, asset(call_obj.collateral, bid.inv_swan_price.base.asset_id),
                                                asset(debt_covered, bid.inv_swan_price.quote.asset_id)));
   set_applied_operation_result(op_id,object_id_result(operation_history_id_type(op_id)));

   remove(bid);
}

void database::cancel_order(const force_settlement_object &order, bool create_virtual_op)
{
   adjust_balance(order.owner, order.balance);

   if (create_virtual_op)
   {
      asset_settle_cancel_operation vop;
      vop.settlement = order.id;
      vop.account = order.owner;
      vop.amount = order.balance;
      auto op_id=push_applied_operation(vop);
      set_applied_operation_result(op_id,object_id_result(operation_history_id_type(op_id)));
   }
   remove(order);
}

void database::cancel_order(const limit_order_object &order, bool create_virtual_op)
{
   auto refunded = order.amount_for_sale();

   modify(order.seller(*this).statistics(*this), [&](account_statistics_object &obj) {
      if (refunded.asset_id == asset_id_type())
      {
         obj.total_core_in_orders -= refunded.amount;
      }
   });
   adjust_balance(order.seller, refunded);
   if(order.deferred_fee)
      adjust_balance(order.seller, *order.deferred_fee);
   if (create_virtual_op)
   {
      limit_order_cancel_operation vop;
      vop.order = order.id;
      vop.fee_paying_account = order.seller;
      auto op_id=push_applied_operation(vop);
      set_applied_operation_result(op_id,object_id_result(operation_history_id_type(op_id)));
   }

   remove(order);
}

bool maybe_cull_small_order(database &db, const limit_order_object &order)
{
   /**
    *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
    *  have hit the limit where the seller is asking for nothing in return.  When this
    *  happens we must refund any balance back to the seller, it is too small to be
    *  sold at the sale price.
    *
    *  If the order is a taker order (as opposed to a maker order), so the price is
    *  set by the counterparty, this check is deferred until the order becomes unmatched
    *  (see #555) -- however, detecting this condition is the responsibility of the caller.
    */
   if (order.amount_to_receive().amount == 0)
   {
      //ilog( "applied epsilon logic" );
      db.cancel_order(order);
      return true;
   }
   return false;
}

bool database::apply_order(const limit_order_object &new_order_object, bool allow_black_swan)
{
   auto order_id = new_order_object.id;
   const asset_object &sell_asset = get(new_order_object.amount_for_sale().asset_id);
   const asset_object &receive_asset = get(new_order_object.amount_to_receive().asset_id);

   // Possible optimization: We only need to check calls if both are true:
   // - The new order is at the front of the book
   // - The new order is below the call limit price
   bool called_some = check_call_orders(sell_asset, allow_black_swan, true); // the first time when checking, call order is maker
   called_some |= check_call_orders(receive_asset, allow_black_swan, true);  // the other side, same as above
   if (called_some && !find_object(order_id))                                // then we were filled by call order
      return true;

   const auto &limit_price_idx = get_index_type<limit_order_index>().indices().get<by_price>();

   // TODO: it should be possible to simply check the NEXT/PREV iterator after new_order_object to
   // determine whether or not this order has "changed the book" in a way that requires us to
   // check orders. For now I just lookup the lower bound and check for equality... this is log(n) vs
   // constant time check. Potential optimization.

   auto max_price = ~new_order_object.sell_price;
   auto limit_itr = limit_price_idx.lower_bound(max_price.max());
   auto limit_end = limit_price_idx.upper_bound(max_price);

   bool finished = false;
   while (!finished && limit_itr != limit_end)
   {
      auto old_limit_itr = limit_itr;
      ++limit_itr;
      // match returns 2 when only the old order was fully filled. In this case, we keep matching; otherwise, we stop.
      finished = (match(new_order_object, *old_limit_itr, old_limit_itr->sell_price) != 2);
   }

   //Possible optimization: only check calls if the new order completely filled some old order
   //Do I need to check both assets?
   check_call_orders(sell_asset, allow_black_swan);    // after the new limit order filled some orders on the book,
                                                       // if a call order matches another order, the call order is taker
   check_call_orders(receive_asset, allow_black_swan); // the other side, same as above

   const limit_order_object *updated_order_object = find<limit_order_object>(order_id);
   if (updated_order_object == nullptr)
      return true;
   // before #555 we would have done maybe_cull_small_order() logic as a result of fill_order() being called by match() above
   // however after #555 we need to get rid of small orders -- #555 hardfork defers logic that was done too eagerly before, and
   // this is the point it's deferred to.
   return maybe_cull_small_order(*this, *updated_order_object);
}

/**
 *  Matches the two orders,
 *
 *  @return a bit field indicating which orders were filled (and thus removed)
 *
 *  0 - no orders were matched
 *  1 - bid was filled
 *  2 - ask was filled
 *  3 - both were filled
 */
template <typename OrderType>
int database::match(const limit_order_object &usd, const OrderType &core, const price &match_price)
{
   assert(usd.sell_price.quote.asset_id == core.sell_price.base.asset_id);
   assert(usd.sell_price.base.asset_id == core.sell_price.quote.asset_id);
   assert(usd.for_sale > 0 && core.for_sale > 0);

   auto usd_for_sale = usd.amount_for_sale();
   auto core_for_sale = core.amount_for_sale();

   asset usd_pays, usd_receives, core_pays, core_receives;

   if (usd_for_sale <= core_for_sale * match_price)
   {
      core_receives = usd_for_sale;
      usd_receives = usd_for_sale * match_price;
   }
   else
   {
      //This line once read: assert( core_for_sale < usd_for_sale * match_price );
      //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
      //Although usd_for_sale is greater than core_for_sale * match_price, core_for_sale == usd_for_sale * match_price
      //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
      usd_receives = core_for_sale;
      core_receives = core_for_sale * match_price;
   }

   core_pays = usd_receives;
   usd_pays = core_receives;

   assert(usd_pays == usd.amount_for_sale() ||
          core_pays == core.amount_for_sale());

   int result = 0;
   result |= fill_order(usd, usd_pays, usd_receives, false, match_price, false);       // although this function is a template,
                                                                                       // right now it only matches one limit order
                                                                                       // with another limit order,
                                                                                       // the first param is a new order, thus taker
   result |= fill_order(core, core_pays, core_receives, true, match_price, true) << 1; // the second param is maker
   assert(result != 0);
   return result;
}

int database::match(const limit_order_object &bid, const limit_order_object &ask, const price &match_price)
{
   return match<limit_order_object>(bid, ask, match_price);
}

asset database::match(const call_order_object &call,
                      const force_settlement_object &settle,
                      const price &match_price,
                      asset max_settlement)
{
   try
   {
      FC_ASSERT(call.get_debt().asset_id == settle.balance.asset_id);
      FC_ASSERT(call.debt > 0 && call.collateral > 0 && settle.balance.amount > 0);

      auto settle_for_sale = std::min(settle.balance, max_settlement);
      auto call_debt = call.get_debt();

      asset call_receives = std::min(settle_for_sale, call_debt);
      asset call_pays = call_receives * match_price;
      asset settle_pays = call_receives;
      asset settle_receives = call_pays;

      /**
    *  If the least collateralized call position lacks sufficient
    *  collateral to cover at the match price then this indicates a black 
    *  swan event according to the price feed, but only the market 
    *  can trigger a black swan.  So now we must cancel the forced settlement
    *  object.
    */
      GRAPHENE_ASSERT(call_pays < call.get_collateral(), black_swan_exception, "");

      assert(settle_pays == settle_for_sale || call_receives == call.get_debt());

      fill_order(call, call_pays, call_receives, match_price, true);        // call order is maker
      fill_order(settle, settle_pays, settle_receives, match_price, false); // force settlement order is taker

      return call_receives;
   }
   FC_CAPTURE_AND_RETHROW((call)(settle)(match_price)(max_settlement))
}

bool database::fill_order(const limit_order_object &order, const asset &pays, const asset &receives, bool cull_if_small,
                          const price &fill_price, const bool is_maker)
{
   try
   {
      cull_if_small |= false; //(head_block_time() < HARDFORK_555_TIME);

      FC_ASSERT(order.amount_for_sale().asset_id == pays.asset_id);
      FC_ASSERT(pays.asset_id != receives.asset_id);

      const account_object &seller = order.seller(*this);
      const asset_object &recv_asset = receives.asset_id(*this);

      auto issuer_fees = pay_market_fees(recv_asset, receives);
      pay_order(seller, receives - issuer_fees, pays);

      assert(pays.asset_id != receives.asset_id);
      auto op_id=push_applied_operation(fill_order_operation(order.id, order.seller, pays, receives, fill_price, is_maker));
      set_applied_operation_result(op_id,object_id_result(operation_history_id_type(op_id)));
      // conditional because cheap integer comparison may allow us to avoid two expensive modify() and object lookups
      if (pays == order.amount_for_sale())
      {
         remove(order);
         return true;
      }
      else
      {
         modify(order, [&](limit_order_object &b) {
            b.for_sale -= pays.amount;
         });
         if (cull_if_small)
            return maybe_cull_small_order(*this, order);
         return false;
      }
   }
   FC_CAPTURE_AND_RETHROW((order)(pays)(receives))
}

bool database::fill_order(const call_order_object &order, const asset &pays, const asset &receives,
                          const price &fill_price, const bool is_maker)
{
   try
   {
      //idump((pays)(receives)(order));
      FC_ASSERT(order.get_debt().asset_id == receives.asset_id);
      FC_ASSERT(order.get_collateral().asset_id == pays.asset_id);
      FC_ASSERT(order.get_collateral() >= pays);

      optional<asset> collateral_freed;
      modify(order, [&](call_order_object &o) {
         o.debt -= receives.amount;
         o.collateral -= pays.amount;
         if (o.debt == 0)
         {
            collateral_freed = o.get_collateral();
            o.collateral = 0;
         }
      });
      const asset_object &mia = receives.asset_id(*this);
      assert(mia.is_market_issued());

      const asset_dynamic_data_object &mia_ddo = mia.dynamic_asset_data_id(*this);

      modify(mia_ddo, [&](asset_dynamic_data_object &ao) {
         //idump((receives));
         ao.current_supply -= receives.amount;
      });

      const account_object &borrower = order.borrower(*this);
      if (collateral_freed || pays.asset_id == asset_id_type())
      {
         const account_statistics_object &borrower_statistics = borrower.statistics(*this);
         if (collateral_freed)
            adjust_balance(borrower.get_id(), *collateral_freed);

         modify(borrower_statistics, [&](account_statistics_object &b) {
            if (collateral_freed && collateral_freed->amount > 0)
               b.total_core_in_orders -= collateral_freed->amount;
            if (pays.asset_id == asset_id_type())
               b.total_core_in_orders -= pays.amount;

            assert(b.total_core_in_orders >= 0);
         });
      }

      assert(pays.asset_id != receives.asset_id);
      auto op_id= push_applied_operation(fill_order_operation(order.id, order.borrower, pays, receives, fill_price, is_maker));
      set_applied_operation_result(op_id,object_id_result(operation_history_id_type(op_id)));

      if (collateral_freed)
         remove(order);

      return collateral_freed.valid();
   }
   FC_CAPTURE_AND_RETHROW((order)(pays)(receives))
}

bool database::fill_order(const force_settlement_object &settle, const asset &pays, const asset &receives,
                          const price &fill_price, const bool is_maker)
{
   try
   {
      bool filled = false;

      auto issuer_fees = pay_market_fees(get(receives.asset_id), receives);

      if (pays < settle.balance)
      {
         modify(settle, [&pays](force_settlement_object &s) {
            s.balance -= pays;
         });
         filled = false;
      }
      else
      {
         filled = true;
      }
      adjust_balance(settle.owner, receives - issuer_fees);

      assert(pays.asset_id != receives.asset_id);
      auto op_id=push_applied_operation(fill_order_operation(settle.id, settle.owner, pays, receives, fill_price, is_maker));
      set_applied_operation_result(op_id,object_id_result(operation_history_id_type(op_id)));
      if (filled)
         remove(settle);

      return filled;
   }
   FC_CAPTURE_AND_RETHROW((settle)(pays)(receives))
}

/**
 *  Starting with the least collateralized orders, fill them if their
 *  call price is above the max(lowest bid,call_limit).
 *
 *  This method will return true if it filled a short or limit
 *
 *  @param mia - the market issued asset that should be called.
 *  @param enable_black_swan - when adjusting collateral, triggering a black swan is invalid and will throw
 *                             if enable_black_swan is not set to true.
 *  @param for_new_limit_order - true if this function is called when matching call orders with a new limit order
 *
 *  @return true if a margin call was executed.
 */
bool database::check_call_orders(const asset_object &mia, bool enable_black_swan, bool for_new_limit_order)
{
   try
   {
      if (!mia.is_market_issued())
         return false;

      if (check_for_blackswan(mia, enable_black_swan))
         return false;

      const asset_bitasset_data_object &bitasset = mia.bitasset_data(*this);
      //if (bitasset.is_prediction_market)return false;  取消二元预测市场
      if (bitasset.current_feed.settlement_price.is_null())
         return false;

      const call_order_index &call_index = get_index_type<call_order_index>();
      const auto &call_price_index = call_index.indices().get<by_price>();

      const limit_order_index &limit_index = get_index_type<limit_order_index>();
      const auto &limit_price_index = limit_index.indices().get<by_price>();

      // looking for limit orders selling the most USD for the least CORE
      auto max_price = price::max(mia.id, bitasset.options.short_backing_asset);
      // stop when limit orders are selling too little USD for too much CORE
      auto min_price = bitasset.current_feed.max_short_squeeze_price();

      assert(max_price.base.asset_id == min_price.base.asset_id);
      // NOTE limit_price_index is sorted from greatest to least
      auto limit_itr = limit_price_index.lower_bound(max_price);
      auto limit_end = limit_price_index.upper_bound(min_price);

      if (limit_itr == limit_end)
         return false;

      auto call_min = price::min(bitasset.options.short_backing_asset, mia.id);
      auto call_max = price::max(bitasset.options.short_backing_asset, mia.id);
      auto call_itr = call_price_index.lower_bound(call_min);
      auto call_end = call_price_index.upper_bound(call_max);

      bool filled_limit = false;
      bool margin_called = false;

      while (!check_for_blackswan(mia, enable_black_swan) && call_itr != call_end)
      {
         bool filled_call = false;
         price match_price;
         asset usd_for_sale;
         if (limit_itr != limit_end)
         {
            assert(limit_itr != limit_price_index.end());
            match_price = limit_itr->sell_price;
            usd_for_sale = limit_itr->amount_for_sale();
         }
         else
            return margin_called;

         match_price.validate();

         // would be margin called, but there is no matching order #436
         bool feed_protected = (bitasset.current_feed.settlement_price > ~call_itr->call_price);
         if (feed_protected //&& (head_block_time() > HARDFORK_436_TIME)
         )
            return margin_called;

         // would be margin called, but there is no matching order
         if (match_price > ~call_itr->call_price)
            return margin_called;

         /*
       if( feed_protected )
       {
          ilog( "Feed protected margin call executing (HARDFORK_436_TIME not here yet)" );
          idump( (*call_itr) );
          idump( (*limit_itr) );
       }
       */

         // idump((*call_itr));
         // idump((*limit_itr));

         // ilog( "match_price <= ~call_itr->call_price  performing a margin call" );

         margin_called = true;

         auto usd_to_buy = call_itr->get_debt();

         if (usd_to_buy * match_price > call_itr->get_collateral())
         {
            elog("black swan detected");
            edump((enable_black_swan));
            FC_ASSERT(enable_black_swan);
            globally_settle_asset(mia, bitasset.current_feed.settlement_price);
            return true;
         }

         asset call_pays, call_receives, order_pays, order_receives;
         if (usd_to_buy >= usd_for_sale)
         { // fill order
            call_receives = usd_for_sale;
            order_receives = usd_for_sale * match_price;
            call_pays = order_receives;
            order_pays = usd_for_sale;

            filled_limit = true;
            filled_call = (usd_to_buy == usd_for_sale);
         }
         else
         { // fill call
            call_receives = usd_to_buy;
            order_receives = usd_to_buy * match_price;
            call_pays = order_receives;
            order_pays = usd_to_buy;

            filled_call = true;
         }

         FC_ASSERT(filled_call || filled_limit);

         auto old_call_itr = call_itr;
         if (filled_call)
            ++call_itr;
         // when for_new_limit_order is true, the call order is maker, otherwise the call order is taker
         fill_order(*old_call_itr, call_pays, call_receives, match_price, for_new_limit_order);

         auto old_limit_itr = limit_itr;
         if (filled_limit)
            ++limit_itr;
         // when for_new_limit_order is true, the limit order is taker, otherwise the limit order is maker
         fill_order(*old_limit_itr, order_pays, order_receives, true, match_price, !for_new_limit_order);

      } // whlie call_itr != call_end

      return margin_called;
   }
   FC_CAPTURE_AND_RETHROW()
}

void database::pay_order(const account_object &receiver, const asset &receives, const asset &pays)
{
   const auto &balances = receiver.statistics(*this);
   modify(balances, [&](account_statistics_object &b) {
      if (pays.asset_id == asset_id_type())
      {
         b.total_core_in_orders -= pays.amount;
      }
   });
   adjust_balance(receiver.get_id(), receives);
}

asset database::calculate_market_fee(const asset_object &trade_asset, const asset &trade_amount)
{
   assert(trade_asset.id == trade_amount.asset_id);

   if (!trade_asset.charges_market_fees())
      return trade_asset.amount(0);
   if (trade_asset.options.market_fee_percent == 0)
      return trade_asset.amount(0);

   fc::uint128 a(trade_amount.amount.value);
   a *= trade_asset.options.market_fee_percent;
   a /= GRAPHENE_100_PERCENT;
   asset percent_fee = trade_asset.amount(a.to_uint64());

   if (percent_fee.amount > trade_asset.options.max_market_fee)
      percent_fee.amount = trade_asset.options.max_market_fee;

   return percent_fee;
}

asset database::pay_market_fees(const asset_object &recv_asset, const asset &receives)
{
   auto issuer_fees = calculate_market_fee(recv_asset, receives);
   assert(issuer_fees <= receives);
   //Don't dirty undo state if not actually collecting any fees
   if (issuer_fees.amount > 0)
   {
      const auto &recv_dyn_data = recv_asset.dynamic_asset_data_id(*this);
      modify(recv_dyn_data, [&](asset_dynamic_data_object &obj) {
         obj.accumulated_fees += issuer_fees.amount;
      });
   }
   return issuer_fees;
}
asset database::estimation_gas(const asset& delta_collateral)
{
   FC_ASSERT(delta_collateral.asset_id==asset_id_type()&&delta_collateral.amount>=0);
   auto core_current_supply = asset_id_type(0)(*this).dynamic_asset_data_id(*this).current_supply;
   auto asset_to_real = [&](const asset &a, int p=0) { return double(a.amount.value) / pow(10, p); };
   double scale=asset_to_real(delta_collateral)/asset_to_real(asset(core_current_supply));
   share_type amount=double(GRAPHENE_ASSET_GAS(*this).options.max_supply.value)*scale;
   return asset(amount,GRAPHENE_ASSET_GAS);
}


} // namespace chain
} // namespace graphene
