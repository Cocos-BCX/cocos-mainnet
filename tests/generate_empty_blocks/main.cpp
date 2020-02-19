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
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/chain/database.hpp>
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
            ("data-dir", bpo::value<boost::filesystem::path>()->default_value("empty_blocks_data_dir"), "Directory containing generator database")
            ("genesis-json,g", bpo::value<boost::filesystem::path>(), "File to read genesis state from")
            ("genesis-time,t", bpo::value<uint32_t>()->default_value(0), "Timestamp for genesis state (0=use value from file/example)")
            ("num-blocks,n", bpo::value<uint32_t>()->default_value(1000000), "Number of blocks to generate")
            ("miss-rate,r", bpo::value<uint32_t>()->default_value(5), "Percentage of blocks to miss")
            ("verbose,v", "Enter verbose mode")
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
         return 0;
      }

      fc::path data_dir;
      if( options.count("data-dir") )
      {
         data_dir = options["data-dir"].as<boost::filesystem::path>();
         if( data_dir.is_relative() )
            data_dir = fc::current_path() / data_dir;
      }

      genesis_state_type genesis;
      if( options.count("genesis-json") )
      {
         fc::path genesis_json_filename = options["genesis-json"].as<boost::filesystem::path>();
         std::cerr << "embed_genesis:  Reading genesis from file " << genesis_json_filename.preferred_string() << "\n";
         std::string genesis_json;
         read_file_contents( genesis_json_filename, genesis_json );
         genesis = fc::json::from_string( genesis_json ).as< genesis_state_type >();
      }
      else {
         wdump(("use default genesis generate by create_example_genesis()"));
         genesis = graphene::app::detail::create_example_genesis();
      }
      uint32_t timestamp = options["genesis-time"].as<uint32_t>();
      if( timestamp != 0 )
      {
         genesis.initial_timestamp = fc::time_point_sec( timestamp );
         std::cerr << "embed_genesis:  Genesis timestamp is " << genesis.initial_timestamp.sec_since_epoch() << " (from CLI)\n";
      }
      else
         std::cerr << "embed_genesis:  Genesis timestamp is " << genesis.initial_timestamp.sec_since_epoch() << " (from state)\n";
      bool verbose = (options.count("verbose") != 0);

      uint32_t num_blocks = options["num-blocks"].as<uint32_t>();
      uint32_t miss_rate = options["miss-rate"].as<uint32_t>();
      fc::ecc::private_key nico_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nico")));

      fc::path db_path = data_dir / "db";
      database db(db_path);
      db.open(db_path, [&]() { return genesis; }, "TEST" );

      uint32_t slot = 1;
      uint32_t missed = 0;

      for( uint32_t i = 1; i < num_blocks; ++i )
      {
         wdump( ("[generate block] ")(db.head_block_id())( i ) );
         signed_block b = db.generate_block(db.get_slot_time(slot), db.get_scheduled_witness(slot), nico_priv_key, database::skip_nothing);
         FC_ASSERT( db.head_block_id() == b.make_id() );
         fc::sha256 h = b.digest();
         uint64_t rand = h._hash[0];
         slot = 1;
         while(true)
         {
            if( (rand % 100) < miss_rate )
            {
               slot++;
               rand = (rand/100) ^ h._hash[slot&3];
               missed++;
            } else {
               //wdump( ("slot: ")(slot) );
               break;
            }
         }

         witness_id_type prev_witness = b.witness;
         witness_id_type cur_witness = db.get_scheduled_witness(1);
         if( verbose )
         {
            wdump( (prev_witness)(cur_witness) );
         }
         else if( (i%10000) == 0 )
         {
            std::cerr << "\rblock #" << i << "   missed " << missed;
         }
         if( slot == 1 )  // can possibly get consecutive production if block missed
         {
            FC_ASSERT( cur_witness != prev_witness );
         }
      }
      std::cerr << "\n";
      db.close();
   }
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
      return 1;
   }
   return 0;
}
