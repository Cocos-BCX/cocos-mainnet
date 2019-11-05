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
#include <graphene/witness/witness.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/witness_object.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>

using namespace graphene::witness_plugin;
using std::string;
using std::vector;

namespace bpo = boost::program_options;

#define start_logo  "\n\
      ********************************\n  \
      *                              *\n  \
      *   ---- Graphene CHAIN ----   *\n  \
      *   - Welcome to ${system} -   *\n  \
      *   ------------------------   *\n  \
      *                              *\n  \
      ********************************\n  \
      \n"

void witness_plugin::new_chain_banner( const graphene::chain::database& db )
{
     ilog(start_logo,("system",GRAPHENE_ADDRESS_PREFIX));
}
void witness_plugin::plugin_set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options)
{
   auto default_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("nico")));
   string witness_id_example = fc::json::to_string(chain::witness_id_type(5));
   command_line_options.add_options()
         ("enable-stale-production", bpo::bool_switch()->notifier([this](bool e){_production_enabled = e;}), "Enable block production, even if the chain is stale.")
         ("required-participation", bpo::bool_switch()->notifier([this](int e){
               _required_witness_participation = uint32_t(e*GRAPHENE_51_PERCENT);}
               ),"Percent of witnesses (0-99) that must be participating in order to produce blocks")
         ("witness-id,w", bpo::value<vector<string>>()->composing()->multitoken(),
          ("ID of witness controlled by this node (e.g. " + witness_id_example + ", quotes are required, may specify multiple times)").c_str())
         ("private-key", bpo::value<vector<string>>()->composing()->multitoken()->
          DEFAULT_VALUE_VECTOR(std::make_pair(chain::public_key_type(default_priv_key.get_public_key()), graphene::utilities::key_to_wif(default_priv_key))),
          "Tuple of [PublicKey, WIF private key] (may specify multiple times),The owner of the private key is nico")
         ("message_cache_limit", boost::program_options::value<uint16_t>()->default_value(3000), "Set the message delivery queue length, At least not less than 3000")
         ("concerned_candidates",bpo::value<string>()->composing(), "Set up candidates to be followed,eg:[\"0:1\",\"1:5\"]")
         ("deduce_in_verification_mode", boost::program_options::value<bool>()->default_value(false), "Whether to deduce in verification mode");
   config_file_options.add(command_line_options);
}

std::string witness_plugin::plugin_name()const
{
   return "witness";
}

void witness_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   ilog("witness plugin:  plugin_initialize() begin");
   _options = &options;
   LOAD_VALUE_SET(options, "witness-id", _witnesses, chain::witness_id_type)

   if( options.count("private-key") )
   {
      const std::vector<std::string> key_id_to_wif_pair_strings = options["private-key"].as<std::vector<std::string>>();
      for (const std::string& key_id_to_wif_pair_string : key_id_to_wif_pair_strings)
      {
         auto key_id_to_wif_pair = graphene::app::dejsonify<std::pair<chain::public_key_type, std::string> >(key_id_to_wif_pair_string);
         //idump((key_id_to_wif_pair));
         ilog("Public Key: ${public}", ("public", key_id_to_wif_pair.first));
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
   ilog("witness plugin:  plugin_initialize() end");
} FC_LOG_AND_RETHROW() }

void witness_plugin::plugin_startup()       ////   witness  启动witness 插件
{ try {
   ilog("witness plugin:  plugin_startup() begin");
   chain::database& d = database();
   //get_message_cache_size_connection=app().chain_database()->get_message_send_cache_size.connect([this](){return this->app().p2p_node()->get_message_cache_size();});
   app().chain_database()->applied_block.connect([this](graphene::chain::signed_block block){return this->app().p2p_node()->set_block_clock();});
   if( !_witnesses.empty() )
   {
      ilog("Launching block production for ${n} witnesses.", ("n", _witnesses.size()));
      app().set_block_production(true);
      if( _production_enabled )
      {
         if( d.head_block_num() == 0 )
            new_chain_banner(d);
         _production_skip_flags |= graphene::chain::database::skip_undo_history_check;
      }
      schedule_production_loop();     //安排生产计划
   } else
   {
      network_quality_test();     //nico add 网络测试
      elog("No witnesses configured! Please add witness IDs and private keys to configuration.");
      }
   ilog("witness plugin:  plugin_startup() end");
} FC_CAPTURE_AND_RETHROW() }

void witness_plugin::network_quality_test()
{
      fc::time_point now = fc::time_point::now();
      int64_t time_to_next_second = 1000000 - (now.time_since_epoch().count() % 1000000);
      if( time_to_next_second < 50000 )      // we must sleep for at least 50ms
             time_to_next_second += 1000000;
      fc::time_point next_wakeup( now + fc::microseconds( time_to_next_second ) );
      _network_quality_testing = fc::schedule([this]{
            chain::database& db = database();
            fc::time_point now_fine = fc::time_point::now();
            fc::time_point_sec now = now_fine + fc::microseconds( 500000 );
            uint32_t prate = db.witness_participation_rate();
            if(_required_witness_participation!=0){
                  if((now.sec_since_epoch()-db.head_block_time().sec_since_epoch())>30)
                  {
                        db.allowe_continue_transaction(0,false);
                        elog("The current network quality is poor, and any transaction will be refused. \
                        Block time interval is more than 30 seconds" );
                  }
                  else
                  {      
                        db.allowe_continue_transaction(prate,prate > _required_witness_participation);
                        if(prate < _required_witness_participation)
                        ilog("The current network quality is poor, and any transaction will be refused. \
                        node appears to be on a minority fork with only ${pct}/10000 witness participation", ("pct",prate) );
                  }
                  network_quality_test();
            }
            
            
       },                      //nico add ：： 开启网络质量测试线程
      next_wakeup, "network quality test");
}

void witness_plugin::plugin_shutdown()
{
   return;
}

void witness_plugin::schedule_production_loop()
{
   //Schedule for the next second's tick regardless of chain state
   // If we would wait less than 50ms, wait for the whole second.
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next_second = 1000000 - (now.time_since_epoch().count() % 1000000);
   if( time_to_next_second < 50000 )      // we must sleep for at least 50ms
       time_to_next_second += 1000000;

   fc::time_point next_wakeup( now + fc::microseconds( time_to_next_second ) );

   //wdump( (now.time_since_epoch().count())(next_wakeup.time_since_epoch().count()) );
   _block_production_task = fc::schedule([this]{block_production_loop();},                      //  ：： 开启生产线程
                                         next_wakeup, "Witness Block Production");
}

block_production_condition::block_production_condition_enum witness_plugin::block_production_loop()
{
   block_production_condition::block_production_condition_enum result;
   fc::mutable_variant_object capture;
   try
   {                                           
      result = maybe_produce_block(capture);   // 是否应该出块 witness maybe produce block
   }
   catch( const fc::canceled_exception& )
   {
      //We're trying to exit. Go ahead and let this one out.
      throw;
   }
   catch( const fc::exception& e )
   {
      elog("Got exception while generating block:\n${e}", ("e", e.to_detail_string()));
      result = block_production_condition::exception_producing_block;
   }

   switch( result )
   {
      case block_production_condition::produced:
         ilog("Generated block #${n} with timestamp ${t} at time ${c}", (capture));
         break;
      case block_production_condition::not_synced:
         ilog("Not producing block because production is disabled until we receive a recent block (see: --enable-stale-production)");
         break;
      case block_production_condition::not_my_turn:
         //ilog("Not producing block because it isn't my turn");
         break;
      case block_production_condition::not_time_yet:
         //ilog("Not producing block because slot has not yet arrived");
         break;
      case block_production_condition::no_private_key:
         ilog("Not producing block because I don't have the private key for ${scheduled_key}", (capture) );
         break;
      case block_production_condition::low_participation:
         elog("The current network quality is poor, and any transaction will be refused. node appears to be on a minority fork with only ${pct}/10000 witness participation", (capture) );
         break;
      case block_production_condition::lag:
         elog("Not producing block because node didn't wake up within 500ms of the slot time.");
         break;
      case block_production_condition::consecutive:
         elog("Not producing block because the last block was generated by the same witness.\nThis node is probably disconnected from the network so block production has been disabled.\nDisable this check with --allow-consecutive option.");
         break;
      case block_production_condition::exception_producing_block:
         elog( "exception prodcing block" );
         break;
   }

   schedule_production_loop();
   return result;
}

block_production_condition::block_production_condition_enum witness_plugin::maybe_produce_block( fc::mutable_variant_object& capture )
{
   chain::database& db = database();
   fc::time_point now_fine = fc::time_point::now();
   fc::time_point_sec now = now_fine + fc::microseconds( 500000 );

   // If the next block production opportunity is in the present or future, we're synced.
   if( !_production_enabled )
   {
      if( db.get_slot_time(1) >= now )
         _production_enabled = true;
      else
         return block_production_condition::not_synced;
   }

   // is anyone scheduled to produce now or one second in the future?
   uint32_t slot = db.get_slot_at_time( now );
   if( slot == 0 )
   {
      capture("next_time", db.get_slot_time(1));
      return block_production_condition::not_time_yet;
   }

   //
   // this assert should not fail, because now <= db.head_block_time()
   // should have resulted in slot == 0.
   //
   // if this assert triggers, there is a serious bug in get_slot_at_time()
   // which would result in allowing a later block to have a timestamp
   // less than or equal to the previous block
   //
   assert( now > db.head_block_time() );
      
   graphene::chain::witness_id_type scheduled_witness = db.get_scheduled_witness( slot );

   uint32_t prate = db.witness_participation_rate();
   //idump((_required_witness_participation)(prate));

   // we must control the witness scheduled to produce the next block.
   if( _witnesses.find( scheduled_witness ) == _witnesses.end() )
   {
      capture("scheduled_witness", scheduled_witness);
      return block_production_condition::not_my_turn;
   }

   fc::time_point_sec scheduled_time = db.get_slot_time( slot );
   graphene::chain::public_key_type scheduled_key = scheduled_witness( db ).signing_key;
   auto private_key_itr = _private_keys.find( scheduled_key );

   if( private_key_itr == _private_keys.end() )
   {
      capture("scheduled_key", scheduled_key);
      return block_production_condition::no_private_key;
   }
   if( llabs((scheduled_time - now).count()) > fc::milliseconds( 500 ).count() )
   {
      capture("scheduled_time", scheduled_time)("now", now);
      return block_production_condition::lag;
   }
                                    
   auto block = db.generate_block(       //  生产区块
      scheduled_time,
      scheduled_witness,
      private_key_itr->second,
      _production_skip_flags
      );                           
   capture("n", block.block_num())("t", block.timestamp)("c", now);
   fc::async( [this,block](){ p2p_node().broadcast(net::block_message(block)); } );  //  广播新生产的区块
   if( prate < _required_witness_participation )
   {
      db.allowe_continue_transaction(prate,false);
      ilog("The current network quality is poor, and any transaction will be refused. \
                  node appears to be on a minority fork with only ${pct}/10000 witness participation", ("pct",prate) );;
   }
   else
      db.allowe_continue_transaction(prate,true);
   return block_production_condition::produced;
}
