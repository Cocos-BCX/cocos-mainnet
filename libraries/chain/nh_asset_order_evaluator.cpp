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
#include <graphene/chain/nh_asset_order_evaluator.hpp>
#include <graphene/chain/nh_asset_object.hpp>
#include <graphene/chain/nh_asset_order_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/nh_asset_creator_object.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <fc/smart_ref_impl.hpp>

#include <fc/log/logger.hpp>


namespace graphene { namespace chain {

void_result create_nh_asset_order_evaluator::do_evaluate(const create_nh_asset_order_operation& o)
{
   database& d = db();
   FC_ASSERT( d.find_object(o.nh_asset) , "Could not find nh asset matching ${nh_asset}", ("nh_asset", o.nh_asset));
   FC_ASSERT( o.nh_asset(d).nh_asset_owner == o.seller , "You’re not the item's owner." );

   FC_ASSERT( o.expiration >= d.head_block_time(), "Order has already expired on creation" );
   FC_ASSERT( o.expiration <= d.head_block_time() + d.get_global_properties().parameters.maximum_nh_asset_order_expiration, "the expiration must less than the maximun expiration." );

   const account_object& from_account    = o.seller(d);
   const account_object& to_account      = o.otcaccount(d);
   const asset_object&   asset_type      = o.pending_orders_fee.asset_id(d);

   try {

      GRAPHENE_ASSERT(
         is_authorized_asset( d, from_account, asset_type ),
         transfer_from_account_not_whitelisted,
         "'from' account ${from} is not whitelisted for asset ${asset}",
         ("from",o.seller)
         ("asset",o.pending_orders_fee.asset_id)
         );
      GRAPHENE_ASSERT(
         is_authorized_asset( d, to_account, asset_type ),
         transfer_to_account_not_whitelisted,
         "'to' account ${to} is not whitelisted for asset ${asset}",
         ("to",o.otcaccount)
         ("asset",o.pending_orders_fee.asset_id)
         );

      if( asset_type.is_transfer_restricted()&&(!asset_type.is_white_list()) )
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset ${asset} has transfer_restricted flag enabled",
            ("asset", o.pending_orders_fee.asset_id)
          );
      }

      bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= o.pending_orders_fee.amount;
      FC_ASSERT( insufficient_balance,
                 "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'", 
                 ("a",from_account.name)("t",to_account.name)("total_transfer",d.to_pretty_string(o.pending_orders_fee))("balance",d.to_pretty_string(d.get_balance(from_account, asset_type))) );

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to pay ${a} from ${f} to ${t}", ("a",d.to_pretty_string(o.pending_orders_fee.amount))("f",o.seller(d).name)("t",o.otcaccount(d).name) );
   return void_result();
}

object_id_result create_nh_asset_order_evaluator::do_apply(const create_nh_asset_order_operation& o)
{ 
   try 
   {
	   database& d = db();
	   const nh_asset_object& nh_asset_obj = o.nh_asset(d);
	   const nh_asset_order_object& nh_asset_order_obj = d.create<nh_asset_order_object>([&](nh_asset_order_object& nh_asset_order) {
		       nh_asset_order.seller = o.seller;
			   nh_asset_order.otcaccount = o.otcaccount;
			   nh_asset_order.nh_asset_id = o.nh_asset;
		       nh_asset_order.asset_qualifier = nh_asset_obj.asset_qualifier;
			   nh_asset_order.world_view = nh_asset_obj.world_view;
			   nh_asset_order.base_describe = nh_asset_obj.base_describe;
			   nh_asset_order.nh_hash = nh_asset_obj.nh_hash;
			   nh_asset_order.price = o.price;
			   nh_asset_order.memo = o.memo;
			   nh_asset_order.expiration = o.expiration;
	       }) ;
	   d.modify(nh_asset_obj, [&](nh_asset_object& g){
	   	   g.nh_asset_owner = GRAPHENE_NULL_ACCOUNT;//nico chang::道具暂时归属到托管账户
	   	   g.nh_asset_active = GRAPHENE_NULL_ACCOUNT;
		   g.dealership = GRAPHENE_NULL_ACCOUNT;
		   });

	   db().adjust_balance( o.seller, -o.pending_orders_fee );
       db().adjust_balance( o.otcaccount, o.pending_orders_fee );
		   
	   return nh_asset_order_obj.id;
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}

void_result cancel_nh_asset_order_evaluator::do_evaluate(const cancel_nh_asset_order_operation& o)
{
   database& d = db();
   FC_ASSERT( d.find_object(o.order) , "Could not find nh asset order matching ${order}", ("order", o.order));
   const nh_asset_order_object& order_obj = o.order(d);
   FC_ASSERT( order_obj.seller == o.fee_paying_account , "You’re not the order's owner." );

   
   return void_result();
}

object_id_result cancel_nh_asset_order_evaluator::do_apply(const cancel_nh_asset_order_operation& o)
{ 
   try 
   {
	   database& d = db();
	   const nh_asset_order_object& order_obj = o.order(d);
       const auto& nh_asset_idx_by_id = d.get_index_type<nh_asset_index>().indices().get<by_nh_asset_hash_id>();
       const auto& nh_asset_idx = nh_asset_idx_by_id.find( order_obj.nh_hash );
	   
	   d.modify(*nh_asset_idx, [&](nh_asset_object& g){
	   	   g.nh_asset_owner = order_obj.seller;
		   g.nh_asset_active = order_obj.seller;
		   g.dealership = order_obj.seller;
		   });

	   d.remove(order_obj);
		   
	   return nh_asset_idx->id;
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}

void_result fill_nh_asset_order_evaluator::do_evaluate(const fill_nh_asset_order_operation& o)
{
   database& d = db();
   FC_ASSERT( d.find_object(o.order) , "Could not find nh asset order matching ${order}", ("order", o.order));
   const nh_asset_order_object& order_obj = o.order(d);

   const account_object& from_account    = o.fee_paying_account(d);
   const account_object& to_account      = o.seller(d);
   const asset_object&   asset_type      = order_obj.price.asset_id(d);

   try {

      GRAPHENE_ASSERT(
         is_authorized_asset( d, from_account, asset_type ),
         transfer_from_account_not_whitelisted,
         "'from' account ${from} is not whitelisted for asset ${asset}",
         ("from",o.fee_paying_account)
         ("asset",order_obj.price.asset_id)
         );
      GRAPHENE_ASSERT(
         is_authorized_asset( d, to_account, asset_type ),
         transfer_to_account_not_whitelisted,
         "'to' account ${to} is not whitelisted for asset ${asset}",
         ("to",o.seller)
         ("asset",order_obj.price.asset_id)
         );

      if( asset_type.is_transfer_restricted()&&(!asset_type.is_white_list()) )
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset ${asset} has transfer_restricted flag enabled",
            ("asset", order_obj.price.asset_id)
          );
      }

      bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= order_obj.price.amount;
      FC_ASSERT( insufficient_balance,
                 "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'", 
                 ("a",from_account.name)("t",to_account.name)("total_transfer",d.to_pretty_string(order_obj.price))("balance",d.to_pretty_string(d.get_balance(from_account, asset_type))) );

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to pay ${a} from ${f} to ${t}", ("a",d.to_pretty_string(order_obj.price))("f",o.fee_paying_account(d).name)("t",o.seller(d).name) );

   
   return void_result();
}

object_id_result fill_nh_asset_order_evaluator::do_apply(const fill_nh_asset_order_operation& o)
{ 
   try 
   {
	   database& d = db();
	   const nh_asset_order_object& order_obj = o.order(d);
       const auto& nh_asset_idx_by_id = d.get_index_type<nh_asset_index>().indices().get<by_nh_asset_hash_id>();
       const auto& nh_asset_idx = nh_asset_idx_by_id.find( order_obj.nh_hash );
	   
	   d.modify(*nh_asset_idx, [&](nh_asset_object& g){
	   	   g.nh_asset_owner = o.fee_paying_account;
		   g.nh_asset_active = o.fee_paying_account;
		   g.dealership = o.fee_paying_account;
		   });
       db().adjust_balance( o.fee_paying_account, -order_obj.price );
       db().adjust_balance( o.seller, order_obj.price );
	   
	   d.remove(order_obj);
		   
	   return nh_asset_idx->id;
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}



} } // graphene::chain
