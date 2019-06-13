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

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <fc/thread/future.hpp>
#include <fc/uint128.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace market_history {
using namespace chain;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
//#ifndef MARKET_HISTORY_SPACE_ID
//#define MARKET_HISTORY_SPACE_ID 5
//#endif

enum market_history_object_type
{
   order_history_object_type = 0,
   bucket_object_type = 1,
   market_ticker_object_type = 2,
   market_ticker_meta_object_type = 3
};

struct bucket_key
{
   bucket_key( asset_id_type a, asset_id_type b, uint32_t s, fc::time_point_sec o )
   :base(a),quote(b),seconds(s),open(o){}
   bucket_key(){}

   asset_id_type      base;
   asset_id_type      quote;
   uint32_t           seconds = 0;
   fc::time_point_sec open;

   friend bool operator < ( const bucket_key& a, const bucket_key& b )
   {
      return std::tie( a.base, a.quote, a.seconds, a.open ) < std::tie( b.base, b.quote, b.seconds, b.open );
   }
   friend bool operator == ( const bucket_key& a, const bucket_key& b )
   {
      return std::tie( a.base, a.quote, b.seconds, a.open ) == std::tie( b.base, b.quote, b.seconds, b.open );
   }
};

struct bucket_object : public abstract_object<bucket_object>
{
   static const uint8_t space_id = MARKET_HISTORY_SPACE_ID;
   static const uint8_t type_id  = bucket_object_type;

   price high()const { return asset( high_base, key.base ) / asset( high_quote, key.quote ); }
   price low()const { return asset( low_base, key.base ) / asset( low_quote, key.quote ); }

   bucket_key          key;
   share_type          high_base;
   share_type          high_quote;
   share_type          low_base;
   share_type          low_quote;
   share_type          open_base;
   share_type          open_quote;
   share_type          close_base;
   share_type          close_quote;
   share_type          base_volume;
   share_type          quote_volume;
};

struct history_key {
  asset_id_type        base;
  asset_id_type        quote;
  int64_t              sequence = 0;

  friend bool operator < ( const history_key& a, const history_key& b ) {
    return std::tie( a.base, a.quote, a.sequence ) < std::tie( b.base, b.quote, b.sequence );
  }
  friend bool operator == ( const history_key& a, const history_key& b ) {
    return std::tie( a.base, a.quote, a.sequence ) == std::tie( b.base, b.quote, b.sequence );
  }
};
struct order_history_object : public abstract_object<order_history_object>
{
   static const uint8_t space_id = MARKET_HISTORY_SPACE_ID;
   static const uint8_t type_id  = order_history_object_type;

   history_key          key;
   fc::time_point_sec   time;
   fill_order_operation op;
};
struct order_history_object_key_base_extractor
{
   typedef asset_id_type result_type;
   result_type operator()(const order_history_object& o)const { return o.key.base; }
};
struct order_history_object_key_quote_extractor
{
   typedef asset_id_type result_type;
   result_type operator()(const order_history_object& o)const { return o.key.quote; }
};
struct order_history_object_key_sequence_extractor
{
   typedef int64_t result_type;
   result_type operator()(const order_history_object& o)const { return o.key.sequence; }
};

struct market_ticker_object : public abstract_object<market_ticker_object>
{
   static const uint8_t space_id = MARKET_HISTORY_SPACE_ID;
   static const uint8_t type_id  = market_ticker_object_type;

   asset_id_type       base;
   asset_id_type       quote;
   share_type          last_day_base;
   share_type          last_day_quote;
   share_type          latest_base;
   share_type          latest_quote;
   fc::uint128         base_volume;
   fc::uint128         quote_volume;
};

struct market_ticker_meta_object : public abstract_object<market_ticker_meta_object>
{
   static const uint8_t space_id = MARKET_HISTORY_SPACE_ID;
   static const uint8_t type_id  = market_ticker_meta_object_type;

   object_id_type      rolling_min_order_his_id;
   bool                skip_min_order_his_id = false;
};

struct by_key;
typedef multi_index_container<
   bucket_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_key>, member< bucket_object, bucket_key, &bucket_object::key > >
   >
> bucket_object_multi_index_type;

struct by_market_time;
typedef multi_index_container<
   order_history_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_key>, member< order_history_object, history_key, &order_history_object::key > >,
      ordered_unique<
         tag<by_market_time>,
         composite_key<
            order_history_object,
            order_history_object_key_base_extractor,
            order_history_object_key_quote_extractor,
            member<order_history_object, time_point_sec, &order_history_object::time>,
            order_history_object_key_sequence_extractor
         >,
         composite_key_compare<
            std::less< asset_id_type >,
            std::less< asset_id_type >,
            std::greater< time_point_sec >,
            std::less< int64_t >
         >
      >
   >
> order_history_multi_index_type;

struct by_market;
typedef multi_index_container<
   market_ticker_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique<
         tag<by_market>,
         composite_key<
            market_ticker_object,
            member<market_ticker_object, asset_id_type, &market_ticker_object::base>,
            member<market_ticker_object, asset_id_type, &market_ticker_object::quote>
         >
      >
   >
> market_ticker_object_multi_index_type;

typedef generic_index<bucket_object, bucket_object_multi_index_type> bucket_index;
typedef generic_index<order_history_object, order_history_multi_index_type> history_index;
typedef generic_index<market_ticker_object, market_ticker_object_multi_index_type> market_ticker_index;


namespace detail
{
    class market_history_plugin_impl;
}

/**
 *  The market history plugin can be configured to track any number of intervals via its configuration.  Once per block it
 *  will scan the virtual operations and look for fill_order_operations and then adjust the appropriate bucket objects for
 *  each fill order.
 */
class market_history_plugin : public graphene::app::plugin
{
   public:
      market_history_plugin();
      virtual ~market_history_plugin();

      std::string plugin_name()const override;
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      virtual void plugin_initialize(
         const boost::program_options::variables_map& options) override;
      virtual void plugin_startup() override;

      uint32_t                    max_history()const;
      const flat_set<uint32_t>&   tracked_buckets()const;
      uint32_t                    max_order_his_records_per_market()const;
      uint32_t                    max_order_his_seconds_per_market()const;

   private:
      friend class detail::market_history_plugin_impl;
      std::unique_ptr<detail::market_history_plugin_impl> my;
};

} } //graphene::market_history

FC_REFLECT( graphene::market_history::history_key, (base)(quote)(sequence) )
FC_REFLECT_DERIVED( graphene::market_history::order_history_object, (graphene::db::object), (key)(time)(op) )
FC_REFLECT( graphene::market_history::bucket_key, (base)(quote)(seconds)(open) )
FC_REFLECT_DERIVED( graphene::market_history::bucket_object, (graphene::db::object),
                    (key)
                    (high_base)(high_quote)
                    (low_base)(low_quote)
                    (open_base)(open_quote)
                    (close_base)(close_quote)
                    (base_volume)(quote_volume) )
FC_REFLECT_DERIVED( graphene::market_history::market_ticker_object, (graphene::db::object),
                    (base)(quote)
                    (last_day_base)(last_day_quote)
                    (latest_base)(latest_quote)
                    (base_volume)(quote_volume) )
FC_REFLECT_DERIVED( graphene::market_history::market_ticker_meta_object, (graphene::db::object),
                    (rolling_min_order_his_id)(skip_min_order_his_id) )
