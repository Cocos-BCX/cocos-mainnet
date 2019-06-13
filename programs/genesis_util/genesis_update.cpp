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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>

#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/app/api.hpp>
#include <graphene/chain/protocol/address.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/utilities/key_conversion.hpp>

#include <boost/filesystem.hpp>

#ifndef WIN32
#include <csignal>
#endif

using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace std;
namespace bpo = boost::program_options;

// hack:  import create_example_genesis() even though it's a way, way
// specific internal detail
namespace graphene { namespace app { namespace detail {
genesis_state_type create_example_genesis();
} } } // graphene::app::detail

int main( int argc, char** argv )
{
   try
   {
      bpo::options_description cli_options("Graphene empty blocks");
      cli_options.add_options()
            ("help,h", "Print this help message and exit.")
            ("genesis-json,g", bpo::value<boost::filesystem::path>(), "File to read genesis state from")
            ("out,o", bpo::value<boost::filesystem::path>(), "File to output new genesis to")
            ("dev-account-prefix", bpo::value<std::string>()->default_value("devacct"), "Prefix for dev accounts")
            ("dev-key-prefix", bpo::value<std::string>()->default_value("devkey-"), "Prefix for dev key")
            ("dev-account-count", bpo::value<uint32_t>()->default_value(0), "Prefix for dev accounts")
            ("dev-balance-count", bpo::value<uint32_t>()->default_value(0), "Prefix for dev balances")
            ("dev-balance-amount", bpo::value<uint64_t>()->default_value(uint64_t(1000)*uint64_t(1000)*uint64_t(100000)), "Amount in each dev balance")
            ;

      bpo::variables_map options;
      try
      {
         boost::program_options::store( boost::program_options::parse_command_line(argc, argv, cli_options), options );
      }
      catch (const boost::program_options::error& e)
      {
         std::cerr << "empty_blocks:  error parsing command line: " << e.what() << "\n";
         return 1;
      }

      if( options.count("help") )
      {
         std::cout << cli_options << "\n";
         return 1;
      }

      if( !options.count( "genesis-json" ) )
      {
         std::cerr << "--genesis-json option is required\n";
         return 1;
      }

      if( !options.count( "out" ) )
      {
         std::cerr << "--out option is required\n";
         return 1;
      }

      genesis_state_type genesis;
      if( options.count("genesis-json") )
      {
         fc::path genesis_json_filename = options["genesis-json"].as<boost::filesystem::path>();
         std::cerr << "update_genesis:  Reading genesis from file " << genesis_json_filename.preferred_string() << "\n";
         std::string genesis_json;
         read_file_contents( genesis_json_filename, genesis_json );
         genesis = fc::json::from_string( genesis_json ).as< genesis_state_type >();
      }
      else
      {
         std::cerr << "update_genesis:  Using example genesis\n";
         genesis = graphene::app::detail::create_example_genesis();
      }

      std::string dev_key_prefix = options["dev-key-prefix"].as<std::string>();

      auto get_dev_key = [&]( std::string prefix, uint32_t i ) -> public_key_type
      {
         return fc::ecc::private_key::regenerate( fc::sha256::hash( dev_key_prefix + prefix + std::to_string(i) ) ).get_public_key();
      };

      uint32_t dev_account_count = options["dev-account-count"].as<uint32_t>();
      std::string dev_account_prefix = options["dev-account-prefix"].as<std::string>();
      for(uint32_t i=0;i<dev_account_count;i++)
      {
         genesis_state_type::initial_account_type acct(
            dev_account_prefix+std::to_string(i),
            get_dev_key( "owner-", i ),
            get_dev_key( "active-", i ),
            false );

         genesis.initial_accounts.push_back( acct );
      }

      uint32_t dev_balance_count = options["dev-balance-count"].as<uint32_t>();
      uint64_t dev_balance_amount = options["dev-balance-amount"].as<uint64_t>();
      for(uint32_t i=0;i<dev_balance_count;i++)
      {
         genesis_state_type::initial_balance_type bal;
         bal.owner = address( get_dev_key( "balance-", i ) );
         bal.asset_symbol = "CORE";
         bal.amount = dev_balance_amount;
         genesis.initial_balances.push_back( bal );
      }

      std::map< std::string, size_t > name2index;
      size_t num_accounts = genesis.initial_accounts.size();
      for( size_t i=0; i<num_accounts; i++ )
         name2index[ genesis.initial_accounts[i].name ] = i;

      for( uint32_t i=0; i<genesis.initial_active_witnesses; i++ )
      {
         genesis_state_type::initial_witness_type& wit = genesis.initial_witness_candidates[ i ];
         genesis_state_type::initial_account_type& wit_acct = genesis.initial_accounts[ name2index[ wit.owner_name ] ];
         if( wit.owner_name.substr(0, 4) != "init" )
         {
            std::cerr << "need " << genesis.initial_active_witnesses << " init accounts as first entries in initial_active_witnesses\n";
            return 1;
         }
         wit.block_signing_key = get_dev_key( "wit-block-signing-", i );
         wit_acct.owner_key = get_dev_key( "wit-owner-", i );
         wit_acct.active_key = get_dev_key( "wit-active-", i );
      }

      fc::path output_filename = options["out"].as<boost::filesystem::path>();
      fc::json::save_to_file( genesis, output_filename );
   }
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
      return 1;
   }
   return 0;
}
