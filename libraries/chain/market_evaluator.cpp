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
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/market_object.hpp>

#include <graphene/chain/market_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/protocol/market.hpp>

#include <fc/uint128.hpp>
#include <fc/smart_ref_impl.hpp>

namespace graphene
{
namespace chain
{
void_result limit_order_create_evaluator::do_evaluate(const limit_order_create_operation &op)
{
   try
   {
      const database &d = db();

      FC_ASSERT(op.expiration >= d.head_block_time());

      _seller = this->fee_paying_account;
      _sell_asset = &op.amount_to_sell.asset_id(d);
      _receive_asset = &op.min_to_receive.asset_id(d);
      FC_ASSERT(op.amount_to_sell.asset_id!=GRAPHENE_ASSET_GAS);

      const auto &index = d.get_index_type<asset_restricted_index>().indices().get<by_asset_and_restricted_enum>();
      const auto &range = index.equal_range(boost::make_tuple(_sell_asset->id, restricted_enum::whitelist_markets));
      if (boost::distance(range))
         FC_ASSERT(index.find(boost::make_tuple(_sell_asset->id, restricted_enum::whitelist_markets, _receive_asset->id)) != index.end());
      FC_ASSERT(index.find(boost::make_tuple(_sell_asset->id, restricted_enum::blacklist_markets, _receive_asset->id)) == index.end());
      /*
   if( _sell_asset->options.whitelist_markets.size() )
      FC_ASSERT( _sell_asset->options.whitelist_markets.find(_receive_asset->id) != _sell_asset->options.whitelist_markets.end() );
   if( _sell_asset->options.blacklist_markets.size() )
      FC_ASSERT( _sell_asset->options.blacklist_markets.find(_receive_asset->id) == _sell_asset->options.blacklist_markets.end() );
   */
      /**************************nico  改**************************************/
      if (_seller->id != _sell_asset->issuer)
      {
         FC_ASSERT(is_authorized_asset(d, *_seller, *_sell_asset));
      }
      /**************************nico * 源*************************************  
   FC_ASSERT( is_authorized_asset( d, *_seller, *_sell_asset ) );
   FC_ASSERT( is_authorized_asset( d, *_seller, *_receive_asset ) );
   *****************************************************************/
      FC_ASSERT(d.get_balance(*_seller, *_sell_asset) >= op.amount_to_sell, "insufficient balance",
                ("balance", d.get_balance(*_seller, *_sell_asset))("amount_to_sell", op.amount_to_sell));

      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void limit_order_create_evaluator::pay_fee()
{
   generic_evaluator::pay_fee();
   if(!filled)
   {   
      auto fees=fee_visitor.fees;
      limit_order_id_type order_id=result.get<object_id_result>().result; 
      db().modify(order_id(db()),[&fees](limit_order_object &obj) {
         for(auto fee:fees)
            if(fee.asset_id==asset_id_type())
               obj.deferred_fee=fee.amount;
      });
   }
}

object_id_result limit_order_create_evaluator::do_apply(const limit_order_create_operation &op)
{
   try
   {
      const auto &seller_stats = _seller->statistics(db());
      db().modify(seller_stats, [&](account_statistics_object &bal) {
         if (op.amount_to_sell.asset_id == asset_id_type())
         {
            bal.total_core_in_orders += op.amount_to_sell.amount;
         }
      });
      db().adjust_balance(op.seller, -op.amount_to_sell);

      const auto &new_order_object = db().create<limit_order_object>([&](limit_order_object &obj) {
         obj.seller = _seller->id;
         obj.for_sale = op.amount_to_sell.amount;
         obj.sell_price = op.get_price();
         obj.expiration = op.expiration;
      });
      //limit_order_id_type order_id = new_order_object.id; // save this because we may remove the object by filling it
      filled = db().apply_order(new_order_object);
      FC_ASSERT(!op.fill_or_kill || filled);

      return new_order_object.id;
   }
   FC_CAPTURE_AND_RETHROW((op))
}

void_result limit_order_cancel_evaluator::do_evaluate(const limit_order_cancel_operation &o)
{
   try
   {
      database &d = db();

      _order = &o.order(d);
      FC_ASSERT(_order->seller == o.fee_paying_account);

      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((o))
}

asset_result limit_order_cancel_evaluator::do_apply(const limit_order_cancel_operation &o)
{
   try
   {
      database &d = db();

      auto base_asset = _order->sell_price.base.asset_id;
      auto quote_asset = _order->sell_price.quote.asset_id;
      auto refunded = _order->amount_for_sale();

      d.cancel_order(*_order, false /* don't create a virtual op*/);

      // Possible optimization: order can be called by canceling a limit order iff the canceled order was at the top of the book.
      // Do I need to check calls in both assets?
      d.check_call_orders(base_asset(d));
      d.check_call_orders(quote_asset(d));

      return refunded;
   }
   FC_CAPTURE_AND_RETHROW((o))
}

void_result call_order_update_evaluator::do_evaluate(const call_order_update_operation &o)
{
   try
   {
      database &d = db();

      _paying_account = &o.funding_account(d);
      _debt_asset = &o.delta_debt.asset_id(d);
      FC_ASSERT(_debt_asset->is_market_issued(), "Unable to cover ${sym} as it is not a collateralized asset.",
                ("sym", _debt_asset->symbol));

      _bitasset_data = &_debt_asset->bitasset_data(d);
      /// if there is a settlement for this asset, then no further margin positions may be taken and
      /// all existing margin positions should have been closed va database::globally_settle_asset

      FC_ASSERT(!_bitasset_data->has_settlement());

      FC_ASSERT(o.delta_collateral.asset_id == _bitasset_data->options.short_backing_asset);
      if (_bitasset_data->current_feed.settlement_price.is_null())
         FC_THROW_EXCEPTION(insufficient_feeds, "Cannot borrow asset with no price feed.");

      if (o.delta_debt.amount < 0)
      {
         FC_ASSERT(d.get_balance(*_paying_account, *_debt_asset) >= o.delta_debt,
                   "Cannot cover by ${c} when payer only has ${b}",
                   ("c", o.delta_debt.amount)("b", d.get_balance(*_paying_account, *_debt_asset).amount));
      }

      if (o.delta_collateral.amount > 0)
      {
         FC_ASSERT(d.get_balance(*_paying_account, _bitasset_data->options.short_backing_asset(d)) >= o.delta_collateral,
                   "Cannot increase collateral by ${c} when payer only has ${b}", ("c", o.delta_collateral.amount)("b", d.get_balance(*_paying_account, o.delta_collateral.asset_id(d)).amount));
      }
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((o))
}
void_result call_order_update_evaluator::do_apply(const call_order_update_operation &o)
{
   try
   {
      database &d = db();
      if (o.delta_debt.amount != 0)
      {
         d.adjust_balance(o.funding_account, o.delta_debt);
         // Deduct the debt paid from the total supply of the debt asset.
         d.modify(_debt_asset->dynamic_asset_data_id(d), [&](asset_dynamic_data_object &dynamic_asset) {
            dynamic_asset.current_supply += o.delta_debt.amount;
            assert(dynamic_asset.current_supply >= 0);
         });
      }
      if (o.delta_collateral.amount != 0)
      {
         d.adjust_balance(o.funding_account, -o.delta_collateral);
         // Adjust the total core in orders accodingly
         if (o.delta_collateral.asset_id == asset_id_type())
         {
            d.modify(_paying_account->statistics(d), [&](account_statistics_object &stats) {
               stats.total_core_in_orders += o.delta_collateral.amount;
            });
         }
      }
      auto &call_idx = d.get_index_type<call_order_index>().indices().get<by_account>();
      auto itr = call_idx.find(boost::make_tuple(o.funding_account, o.delta_debt.asset_id));
      const call_order_object *call_obj = nullptr;
      if (itr == call_idx.end())
      {
         FC_ASSERT(o.delta_collateral.amount > 0);
         FC_ASSERT(o.delta_debt.amount > 0);
         call_obj = &d.create<call_order_object>([&](call_order_object &call) {
            call.borrower = o.funding_account;
            call.collateral = o.delta_collateral.amount;
            call.debt = o.delta_debt.amount;
            call.call_price = price::call_price(o.delta_debt, o.delta_collateral,
                                                _bitasset_data->current_feed.maintenance_collateral_ratio);
         });
      }
      else
      {
         call_obj = &*itr;
         d.modify(*call_obj, [&](call_order_object &call) {
            call.collateral += o.delta_collateral.amount;
            call.debt += o.delta_debt.amount;
            if (call.debt > 0)
               call.call_price = price::call_price(call.get_debt(), call.get_collateral(), _bitasset_data->current_feed.maintenance_collateral_ratio);
         });
      }
      auto debt = call_obj->get_debt();
      if (debt.amount == 0)
      {
         FC_ASSERT(call_obj->collateral == 0);
         d.remove(*call_obj);
         return void_result();
      }
      FC_ASSERT(call_obj->collateral > 0 && call_obj->debt > 0);
      call_order_id_type call_order_id = call_obj->id;
      // check to see if the order needs to be margin called now, but don't allow black swans and require there to be
      // limit orders available that could be used to fill the order.
      if (d.check_call_orders(*_debt_asset, false))
      {
         const auto call_obj = d.find(call_order_id);
         // if we filled at least one call order, we are OK if we totally filled.
         GRAPHENE_ASSERT(!call_obj, call_order_update_unfilled_margin_call,
                         "Updating call order would trigger a margin call that cannot be fully filled",
                         ("a", ~call_obj->call_price)("b", _bitasset_data->current_feed.settlement_price));
      }
      else
      {
         const auto call_obj = d.find(call_order_id);
         FC_ASSERT(call_obj, "no margin call was executed and yet the call object was deleted");
         //edump( (~call_obj->call_price) ("<")( _bitasset_data->current_feed.settlement_price) );
         // We didn't fill any call orders.  This may be because we
         // aren't in margin call territory, or it may be because there
         // were no matching orders.  In the latter case, we throw.
         GRAPHENE_ASSERT(~call_obj->call_price < _bitasset_data->current_feed.settlement_price,
                         call_order_update_unfilled_margin_call,
                         "Updating call order would trigger a margin call that cannot be fully filled",
                         ("a", ~call_obj->call_price)("b", _bitasset_data->current_feed.settlement_price));
      }
      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((o))
}

void_result bid_collateral_evaluator::do_evaluate(const bid_collateral_operation &o)
{
   try
   {
      database &d = db();

      _paying_account = &o.bidder(d);
      _debt_asset = &o.debt_covered.asset_id(d);
      FC_ASSERT(_debt_asset->is_market_issued(), "Unable to cover ${sym} as it is not a collateralized asset.",
                ("sym", _debt_asset->symbol));

      _bitasset_data = &_debt_asset->bitasset_data(d);

      FC_ASSERT(_bitasset_data->has_settlement());

      FC_ASSERT(o.additional_collateral.asset_id == _bitasset_data->options.short_backing_asset);

      //FC_ASSERT( !_bitasset_data->is_prediction_market, "Cannot bid on a prediction market!" ); // 取消二元预测市场

      if (o.additional_collateral.amount > 0)
      {
         FC_ASSERT(d.get_balance(*_paying_account, _bitasset_data->options.short_backing_asset(d)) >= o.additional_collateral,
                   "Cannot bid ${c} collateral when payer only has ${b}", ("c", o.additional_collateral.amount)("b", d.get_balance(*_paying_account, o.additional_collateral.asset_id(d)).amount));
      }

      const collateral_bid_index &bids = d.get_index_type<collateral_bid_index>();
      const auto &index = bids.indices().get<by_account>();
      const auto &bid = index.find(boost::make_tuple(o.debt_covered.asset_id, o.bidder));
      if (bid != index.end())
         _bid = &(*bid);
      else
         FC_ASSERT(o.debt_covered.amount > 0, "Can't find bid to cancel?!");

      return void_result();
   }
   FC_CAPTURE_AND_RETHROW((o))
}

// 出价抵押品评估人
object_id_result bid_collateral_evaluator::do_apply(const bid_collateral_operation &o)
{
   try
   {
      database &d = db();

      if (_bid)
      {   
         d.cancel_bid(*_bid, false);
         if (o.debt_covered.amount == 0)
         return object_id_result(_bid->id);;
      }

      d.adjust_balance(o.bidder, -o.additional_collateral);

      _bid = &d.create<collateral_bid_object>([&](collateral_bid_object &bid) {
         bid.bidder = o.bidder;
         bid.inv_swan_price = o.additional_collateral / o.debt_covered;
      });

      return object_id_result(_bid->id);
   }
   FC_CAPTURE_AND_RETHROW((o))
}

} // namespace chain
} // namespace graphene
