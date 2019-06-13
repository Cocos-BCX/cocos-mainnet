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
#include <graphene/chain/world_view_evaluator.hpp>
#include <graphene/chain/world_view_object.hpp>
#include <graphene/chain/nh_asset_creator_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>

#include <fc/smart_ref_impl.hpp>

#include <fc/log/logger.hpp>
#include <cctype>


namespace graphene { namespace chain {

void_result create_world_view_evaluator::do_evaluate(const create_world_view_operation& o)
{
   try 
   {
	   database& d = db();
	   auto& nh_asset_creator_idx = d.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();

	   FC_ASSERT( nh_asset_creator_idx.find(o.fee_paying_account) != nh_asset_creator_idx.end() , "You're not a nh asset creator, so you can't creat world view." );
	   FC_ASSERT( !std::isdigit(o.world_view[0]) , "world view name can't start whith a digit." );
	   return void_result();
   
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}

object_id_result create_world_view_evaluator::do_apply(const create_world_view_operation& o)
{ 
   try 
   {
	   database& d = db();
       const auto& nh_asset_creator_idx = d.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();
	   const auto& idx = nh_asset_creator_idx.find(o.fee_paying_account);
	   const world_view_object& world_view_obj = d.create<world_view_object>([&](world_view_object& world_view) {
	       world_view.related_nht_creator.emplace_back(idx->id);
		   world_view.world_view = o.world_view;
           world_view.world_view_creator = idx->id;
	       }) ;
	   d.modify(*idx, [&](nh_asset_creator_object& g) {
         g.world_view.emplace_back(world_view_obj.world_view);
         });
	   return world_view_obj.id;
   
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}

void_result relate_world_view_evaluator::do_evaluate(const relate_world_view_operation& o)
{
   try 
   {
      database& d = db();
	  auto& nh_asset_creator_idx = d.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();

	  //Verify that if the proposer is a nht creator
	  const auto& popposer_idx = nh_asset_creator_idx.find(o.related_account);
	  FC_ASSERT( popposer_idx != nh_asset_creator_idx.end() , "You're not a nh asset creator, so you can't create world view." );
	  const auto& owner_idx = nh_asset_creator_idx.find(o.view_owner);
	  //FC_ASSERT( owner_idx != nh_asset_creator_idx.end() , "${owner} is not a nh asset creater.", ("owner", o.view_owner) );

	  //Verify that if the world view exists
	  const auto& version_idx_by_name = d.get_index_type<world_view_index>().indices().get<by_world_view>();
	  const auto& version_idx = version_idx_by_name.find(o.world_view);
	  FC_ASSERT( version_idx != version_idx_by_name.end() , "${version} is not exist.", ("version", o.world_view.c_str()) );

	  //Verify that if the worldview creator is correct
	  FC_ASSERT( version_idx->world_view_creator == owner_idx->id, "${owner} is not the world's creator.", ("owner", o.view_owner) );
	  //Verify that the proposer is already related to the world view
	  FC_ASSERT( version_idx->related_nht_creator.end() == find(version_idx->related_nht_creator.begin(), version_idx->related_nht_creator.end(), 
	            popposer_idx->id), "${popposer} has relate to the world view.", ("popposer", o.related_account) );
      
      return void_result();
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}

void_result relate_world_view_evaluator::do_apply(const relate_world_view_operation& o)
{ 
   try 
   {
      database& d = db();
	  const auto& nh_asset_creator_idx = d.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();
	  const auto& idx = nh_asset_creator_idx.find(o.related_account);
	  
	  const auto& version_idx_by_name = d.get_index_type<world_view_index>().indices().get<by_world_view>();
	  const auto& version_idx = version_idx_by_name.find(o.world_view);
      d.modify(*version_idx, [&](world_view_object& p) {
         p.related_nht_creator.push_back(idx->id);
         });
      d.modify(*idx, [&](nh_asset_creator_object& g) {
         g.world_view.push_back(o.world_view);
         });
      return void_result();
   } 
   FC_CAPTURE_AND_RETHROW( (o) ) 
}

} } // graphene::chain
