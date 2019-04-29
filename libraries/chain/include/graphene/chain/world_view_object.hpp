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

#include <graphene/db/generic_index.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene { namespace chain {

	class world_view_object : public graphene::db::abstract_object<world_view_object>
	{
	   public:
		  static const uint8_t space_id = nh_asset_protocol_ids;
		  static const uint8_t type_id	= world_view_object_type;
		  string world_view;
          nh_asset_creator_id_type world_view_creator;
		  vector<nh_asset_creator_id_type> related_nht_creator;
	};

    struct by_world_view{};
	
	typedef multi_index_container<
	   world_view_object,
	   indexed_by<
		  ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
		  ordered_unique< tag<by_world_view>, member< world_view_object, string, &world_view_object::world_view > >
    >
	> world_view_object_multi_index_type;
	
	typedef generic_index<world_view_object, world_view_object_multi_index_type> world_view_index;


} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::world_view_object,
                    (graphene::db::object), 
                    (world_view)(world_view_creator)(related_nht_creator))

