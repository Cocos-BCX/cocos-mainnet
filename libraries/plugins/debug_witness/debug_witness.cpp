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
#include <graphene/debug_witness/debug_witness.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/witness_object.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>

using namespace graphene::debug_witness_plugin;
using std::string;
using std::vector;

namespace bpo = boost::program_options;

debug_witness_plugin::~debug_witness_plugin() {}

void debug_witness_plugin::plugin_set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options)
{
   auto default_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("nico")));
   command_line_options.add_options()
         ("debug-private-key", bpo::value<vector<string>>()->composing()->multitoken()->
          DEFAULT_VALUE_VECTOR(std::make_pair(chain::public_key_type(default_priv_key.get_public_key()), graphene::utilities::key_to_wif(default_priv_key))),
          "Tuple of [PublicKey, WIF private key] (may specify multiple times), Debugger's name : nico ");
   config_file_options.add(command_line_options);
}

std::string debug_witness_plugin::plugin_name()const
{
   return "debug_witness";
}

void debug_witness_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   ilog("debug_witness plugin:  plugin_initialize() begin");
   _options = &options;

   if( options.count("debug-private-key") )
   {
      const std::vector<std::string> key_id_to_wif_pair_strings = options["debug-private-key"].as<std::vector<std::string>>();
      for (const std::string& key_id_to_wif_pair_string : key_id_to_wif_pair_strings)
      {
         auto key_id_to_wif_pair = graphene::app::dejsonify<std::pair<chain::public_key_type, std::string> >(key_id_to_wif_pair_string);
         idump((key_id_to_wif_pair));
         fc::optional<fc::ecc::private_key> private_key = graphene::utilities::wif_to_key(key_id_to_wif_pair.second);
         if (!private_key)
         {
            // the key isn't in WIF format; see if they are still passing the old native private key format.  This is
            // just here to ease the transition, can be removed soon
            try
            {
               private_key = fc::variant(key_id_to_wif_pair.second).as<fc::ecc::private_key>();
            }
            catch (const fc::exception&)
            {
               FC_THROW("Invalid WIF-format private key ${key_string}", ("key_string", key_id_to_wif_pair.second));
            }
         }
         _private_keys[key_id_to_wif_pair.first] = *private_key;
      }
   }
   ilog("debug_witness plugin:  plugin_initialize() end");
} FC_LOG_AND_RETHROW() }

void debug_witness_plugin::plugin_startup()
{
   ilog("debug_witness_plugin::plugin_startup() begin");
   chain::database& db = database();

   // connect needed signals

   _applied_block_conn  = db.applied_block.connect([this](const graphene::chain::signed_block& b){ on_applied_block(b); });
   _changed_objects_conn = db.changed_objects.connect([this](const std::vector<graphene::db::object_id_type>& ids, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts){ on_changed_objects(ids, impacted_accounts); });
   _removed_objects_conn = db.removed_objects.connect([this](const std::vector<graphene::db::object_id_type>& ids, const std::vector<const graphene::db::object*>& objs, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts){ on_removed_objects(ids, objs, impacted_accounts); });

   return;
}

void debug_witness_plugin::on_changed_objects( const std::vector<graphene::db::object_id_type>& ids, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts )
{
   if( _json_object_stream && (ids.size() > 0) )
   {
      const chain::database& db = database();
      for( const graphene::db::object_id_type& oid : ids )
      {
         const graphene::db::object* obj = db.find_object( oid );
         if( obj != nullptr )
         {
            (*_json_object_stream) << fc::json::to_string( obj->to_variant() ) << '\n';
         }
      }
   }
}

void debug_witness_plugin::on_removed_objects( const std::vector<graphene::db::object_id_type>& ids, const std::vector<const graphene::db::object*> objs, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts )
{
   if( _json_object_stream )
   {
      for( const graphene::db::object* obj : objs )
      {
         (*_json_object_stream) << "{\"id\":" << fc::json::to_string( obj->id ) << "}\n";
      }
   }
}

void debug_witness_plugin::on_applied_block( const graphene::chain::signed_block& b )
{
   if( _json_object_stream )
   {
      (*_json_object_stream) << "{\"bn\":" << fc::to_string( b.block_num() ) << "}\n";
   }
}

void debug_witness_plugin::set_json_object_stream( const std::string& filename )
{
   if( _json_object_stream )
   {
      _json_object_stream->close();
      _json_object_stream.reset();
   }
   _json_object_stream = std::make_shared< std::ofstream >( filename );
}

void debug_witness_plugin::flush_json_object_stream()
{
   if( _json_object_stream )
      _json_object_stream->flush();
}

void debug_witness_plugin::plugin_shutdown()
{
   if( _json_object_stream )
   {
      _json_object_stream->close();
      _json_object_stream.reset();
   }
   return;
}
