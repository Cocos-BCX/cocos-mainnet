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

namespace graphene
{
namespace chain
{

class nh_asset_order_object : public graphene::db::abstract_object<nh_asset_order_object>
{
  
  public:
	class overwrite_asset:public asset
  {
      public:
      overwrite_asset():asset(){};
      overwrite_asset(asset a):asset(a.amount,a.asset_id){};

      friend bool operator == ( const overwrite_asset& a, const overwrite_asset& b )
      {
         return std::tie( a.asset_id, a.amount ) == std::tie( b.asset_id, b.amount );
      }
      friend bool operator < ( const overwrite_asset& a, const overwrite_asset& b )
      {
         if(a.asset_id == b.asset_id )
         return a.amount < b.amount;
         else return a.asset_id<b.asset_id;
      }
      friend bool operator > ( const overwrite_asset& a, const overwrite_asset& b )
      {
         return !(a<b);
      }
  };
  static const uint8_t space_id = nh_asset_protocol_ids;
	static const uint8_t type_id = nh_asset_order_object_type;

	account_id_type seller;
	account_id_type otcaccount;
	nh_asset_id_type nh_asset_id;
	string asset_qualifier; //限定资产符号
	string world_view;
	string base_describe;
	nh_hash_type nh_hash;
	overwrite_asset price;
	string memo;
	time_point_sec expiration = time_point_sec::maximum();

    nh_hash_type get_base_describe_hash() const
    {
      /*
        nh_hash_type result(nh_hash);
        result._hash[0] = result._hash[0] ^ uint64_t(nh_asset_id);
        return result;
        */
       nh_hash_type result(nh_hash);
        result._hash[0] = 0;
        return result;
    }
};

struct by_view_qualifier_describe_and_price_ascending
{};
struct by_view_qualifier_describe_and_price_descending
{};
struct by_nh_asset_seller
{
};
struct by_order_expiration
{
};
struct by_greater_id;
typedef multi_index_container<
	nh_asset_order_object,
	indexed_by<
        ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
        ordered_unique<tag<by_greater_id>, member<object, object_id_type, &object::id>,std::greater<object_id_type>>,
        ordered_unique<tag<by_nh_asset_hash_id>, member<nh_asset_order_object, nh_hash_type, &nh_asset_order_object::nh_hash>>,
        ordered_non_unique<tag<by_nh_asset_seller>,
                           composite_key<nh_asset_order_object,
                                         member<nh_asset_order_object, account_id_type, &nh_asset_order_object::seller>,
                                         member<object, object_id_type, &object::id>>,
                            composite_key_compare<
                                         std::less<account_id_type>,
                                         std::greater<object_id_type>>>,
        ordered_non_unique<tag<by_view_qualifier_describe_and_price_ascending>,
                           composite_key<nh_asset_order_object,
                                         member<nh_asset_order_object, string, &nh_asset_order_object::world_view>,
                                         member<nh_asset_order_object, string, &nh_asset_order_object::asset_qualifier>,
                                         const_mem_fun< nh_asset_order_object, nh_hash_type, &nh_asset_order_object::get_base_describe_hash>,
                                         member<nh_asset_order_object, nh_asset_order_object::overwrite_asset, &nh_asset_order_object::price>>,
                           composite_key_compare<
                                         std::less<std::string>,
                                         std::less<string>,
                                         std::less<nh_hash_type>,
                                         std::less<nh_asset_order_object::overwrite_asset>>>,
        ordered_non_unique<tag<by_view_qualifier_describe_and_price_descending>,
                           composite_key<nh_asset_order_object,
                                         member<nh_asset_order_object, string, &nh_asset_order_object::world_view>,
                                         member<nh_asset_order_object, string, &nh_asset_order_object::asset_qualifier>,
                                         const_mem_fun<nh_asset_order_object, nh_hash_type, &nh_asset_order_object::get_base_describe_hash>,
                                         member<nh_asset_order_object, nh_asset_order_object::overwrite_asset, &nh_asset_order_object::price>>,
                           composite_key_compare<
                                         std::less<std::string>,
                                         std::less<string>,
                                         std::less<nh_hash_type>,
                                         std::greater<nh_asset_order_object::overwrite_asset>>>,
        ordered_non_unique<tag<by_order_expiration>,
                           composite_key<nh_asset_order_object,
                                         member<nh_asset_order_object, time_point_sec, &nh_asset_order_object::expiration>>>>>
	nh_asset_order_object_multi_index_type;

typedef generic_index<nh_asset_order_object, nh_asset_order_object_multi_index_type> nh_asset_order_index;

} // namespace chain
} // namespace graphene


FC_REFLECT_DERIVED(graphene::chain::nh_asset_order_object::overwrite_asset,
				   (graphene::chain::asset),)

FC_REFLECT_DERIVED(graphene::chain::nh_asset_order_object,
				   (graphene::db::object),
				   (seller)(otcaccount)(nh_asset_id)(asset_qualifier)(world_view)(base_describe)(nh_hash)(price)(memo)(expiration))

