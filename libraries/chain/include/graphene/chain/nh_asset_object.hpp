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
#include <fc/io/raw.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene
{
namespace chain
{

enum class nh_asset_lease_limit_type
{
   black_list = 0,
   white_list = 1
};

class nh_asset_object : public graphene::db::abstract_object<nh_asset_object>
{

  public:
	static const uint8_t space_id = nh_asset_protocol_ids;
	static const uint8_t type_id = nh_asset_object_type;

	nh_hash_type nh_hash;
	account_id_type nh_asset_creator;
	account_id_type nh_asset_owner;
	account_id_type nh_asset_active; // the account who has the usage rights of the nht
	account_id_type dealership; // this account has authority to modify the nht's  active by contract api
	string asset_qualifier;
	string world_view;
	string base_describe;
	map<contract_id_type, vector<nh_asset_id_type>> parent;
	map<contract_id_type, vector<nh_asset_id_type>> child;
	map<contract_id_type, map<string, string>> describe_with_contract;
	time_point_sec create_time;
	vector<contract_id_type> limit_list;
	nh_asset_lease_limit_type limit_type = nh_asset_lease_limit_type::black_list;

    nh_hash_type get_base_describe_hash() const
    {
        nh_hash_type result(nh_hash);
        result._hash[0] = 0;
        return result;
    }

	void get_hash()
	{
	    nh_hash_type::encoder enc;
		fc::raw::pack(enc, base_describe);
		nh_hash = enc.result();
        nh_hash._hash[0] = id.instance();
	}

    bool is_leasing() const
    {
        return nh_asset_owner != nh_asset_active;
    }
};

struct by_nh_asset_hash_id
{
};
struct by_nh_asset_view
{
};
struct by_owner_lease_status_and_view
{
};
struct by_active_lease_status_and_view
{
};
struct by_nh_asset_creator;

typedef multi_index_container<
	nh_asset_object,
	indexed_by<
		ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
		hashed_unique<tag<by_nh_asset_hash_id>, member<nh_asset_object, nh_hash_type, &nh_asset_object::nh_hash>>,
		ordered_non_unique<tag<by_nh_asset_view>, member<nh_asset_object, string, &nh_asset_object::world_view>>,
		ordered_non_unique<tag<by_owner_lease_status_and_view>,
						   composite_key<nh_asset_object,
										 member<nh_asset_object, account_id_type, &nh_asset_object::nh_asset_owner>,
										 const_mem_fun< nh_asset_object, bool, &nh_asset_object::is_leasing>,
										 member<nh_asset_object, string, &nh_asset_object::world_view>>>,
		ordered_non_unique<tag<by_active_lease_status_and_view>,
						   composite_key<nh_asset_object,
										 member<nh_asset_object, account_id_type, &nh_asset_object::nh_asset_active>,
										 const_mem_fun< nh_asset_object, bool, &nh_asset_object::is_leasing>,
										 member<nh_asset_object, string, &nh_asset_object::world_view>>>,
		ordered_non_unique<tag<by_nh_asset_creator>, 
						composite_key<nh_asset_object,
						member<nh_asset_object, account_id_type, &nh_asset_object::nh_asset_creator>,
						member<nh_asset_object, string, &nh_asset_object::world_view>
						>
		>>>
	nh_asset_object_multi_index_type;

typedef generic_index<nh_asset_object, nh_asset_object_multi_index_type> nh_asset_index;

} // namespace chain
} // namespace graphene

FC_REFLECT_ENUM(graphene::chain::nh_asset_lease_limit_type, (black_list)(white_list))

FC_REFLECT_DERIVED(graphene::chain::nh_asset_object,
				   (graphene::db::object),
				   (nh_hash)(nh_asset_creator)(nh_asset_owner)(nh_asset_active)(dealership)(asset_qualifier)(world_view)
				   (base_describe)(parent)(child)(describe_with_contract)(create_time)(limit_list)(limit_type))

