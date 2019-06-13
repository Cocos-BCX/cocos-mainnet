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
#include <graphene/chain/nh_asset_creator_evaluator.hpp>
#include <graphene/chain/nh_asset_creator_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>

#include <fc/smart_ref_impl.hpp>

#include <fc/log/logger.hpp>


namespace graphene { namespace chain {

void_result register_nh_asset_creator_evaluator::do_evaluate(const register_nh_asset_creator_operation& o)
{
   try 
   {
	   database& d = db();
	   auto& nh_asset_creator_idx = d.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();

	   FC_ASSERT( nh_asset_creator_idx.find(o.fee_paying_account) == nh_asset_creator_idx.end() , "You had registered to a nh asset creater." );
	   return void_result();
   
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}

object_id_result register_nh_asset_creator_evaluator::do_apply(const register_nh_asset_creator_operation& o)
{ try {
   database& d = db();

   const nh_asset_creator_object& nh_asset_creator_obj = d.create<nh_asset_creator_object>([&](nh_asset_creator_object& nh_asset_creator) {
       nh_asset_creator.creator = o.fee_paying_account;
   }) ;
   return nh_asset_creator_obj.id;
   
} FC_CAPTURE_AND_RETHROW( (o) ) }


} } // graphene::chain
