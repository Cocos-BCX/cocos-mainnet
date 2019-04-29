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

	class nh_asset_creator_object : public graphene::db::abstract_object<nh_asset_creator_object>
	{
	   public:
		  static const uint8_t space_id = nh_asset_protocol_ids;
		  static const uint8_t type_id	= nh_asset_creator_object_type;

		  vector<string> world_view;
		  account_id_type creator;
	};

	struct by_nh_asset_creator{};
	
	typedef multi_index_container<
	   nh_asset_creator_object,
	   indexed_by<
		  ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
		  ordered_unique< tag<by_nh_asset_creator>, member< nh_asset_creator_object, account_id_type, &nh_asset_creator_object::creator > >
    >
	> nh_asset_creator_object_multi_index_type;
	
	typedef generic_index<nh_asset_creator_object, nh_asset_creator_object_multi_index_type> nh_asset_creator_index;


} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::nh_asset_creator_object,
                    (graphene::db::object),
                    (world_view)(creator) )

