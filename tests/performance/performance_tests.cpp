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
#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/protocol.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <graphene/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include "../common/database_fixture.hpp"

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( performance_tests, database_fixture )

BOOST_AUTO_TEST_CASE( sigcheck_benchmark )
{
   fc::ecc::private_key nico_key = fc::ecc::private_key::generate();
   auto digest = fc::sha256::hash("nicotest");
   auto sig = nico_key.sign_compact( digest );
   auto start = fc::time_point::now();
   for( uint32_t i = 0; i < 100000; ++i )
      auto pub = fc::ecc::public_key( sig, digest );
   auto end = fc::time_point::now();
   auto elapsed = end-start;
   wdump( ((100000.0*1000000.0) / elapsed.count()) );
}

// See https://bitshares.org/blog/2015/06/08/measuring-performance/
// (note this is not the original test mentioned in the above post, but was
//  recreated later according to the description)
BOOST_AUTO_TEST_CASE( one_hundred_k_benchmark )
{
   try {
      ACTORS( (alice) );
      fund( alice, asset(10000000) );
      db->_undo_db.disable(); // Blog post mentions replay, this implies no undo

      const fc::ecc::private_key nathan_key = fc::ecc::private_key::generate();
      const fc::ecc::public_key  nathan_pub = nathan_key.get_public_key();;
      const auto& committee_account = account_id_type()(*db);

      const uint64_t cycles = 200000;
      uint64_t total_time = 0;
      uint64_t total_count = 0;
      std::vector<account_id_type> accounts;
      accounts.reserve( cycles+1 );
      std::vector<asset_id_type> assets;
      assets.reserve( cycles );

      std::vector<signed_transaction> transactions;
      transactions.reserve( cycles );

      {
         account_create_operation aco;
         aco.name = "otter";
         aco.registrar = committee_account.id;
         aco.owner = authority( 1, public_key_type(nathan_pub), 1 );
         aco.active = authority( 1, public_key_type(nathan_pub), 1 );
         aco.options.memo_key = nathan_pub;
         trx.clear();
         test::set_expiration( db.get(), trx );
         for( uint32_t i = 0; i < cycles; ++i )
         {
            aco.name = "otter" + fc::to_string(i);
            trx.operations.push_back( aco );
            transactions.push_back( trx );
            trx.operations.clear();
            ++total_count;
         }

         auto start = fc::time_point::now();
         for( uint32_t i = 0; i < cycles; ++i )
         {
            auto result = db->apply_transaction( transactions[i], ~0, transaction_apply_mode::apply_block_mode );
            accounts[i] = result.operation_results[0].get<object_id_result>().result;
         }
         auto end = fc::time_point::now();
         auto elapsed = end - start;
         total_time += elapsed.count();
         wlog( "Create ${aps} accounts/s over ${total}ms",
               ("aps",(cycles*1000000)/elapsed.count())("total",elapsed.count()/1000) );
      }

      {
         accounts[cycles] = accounts[0];
         transfer_operation to1;
         to1.from = committee_account.id;
         to1.amount = asset( 1000000 );
         transfer_operation to2;
         to2.amount = asset( 100 );
         for( uint32_t i = 0; i < cycles; ++i )
         {
            to1.to = accounts[i];
            to2.from = accounts[i];
            to2.to = accounts[i+1];
            trx.operations.push_back( to1 );
            ++total_count;
            trx.operations.push_back( to2 );
            ++total_count;
            transactions[i] = trx;
            trx.operations.clear();
         }

         auto start = fc::time_point::now();
         for( uint32_t i = 0; i < cycles; ++i )
            db->apply_transaction( transactions[i], ~0 );
         auto end = fc::time_point::now();
         auto elapsed = end - start;
         total_time += elapsed.count();
         wlog( "${aps} transfers/s over ${total}ms",
               ("aps",(2*cycles*1000000)/elapsed.count())("total",elapsed.count()/1000) );
         trx.clear();
      }

      {
         asset_create_operation aco;
         aco.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type(1) );
         for( uint32_t i = 0; i < cycles; ++i )
         {
            aco.issuer = accounts[i];
            aco.symbol = "ASSET" + fc::to_string( i );
            trx.operations.push_back( aco );
            ++total_count;
            transactions[i] = trx;
            trx.operations.clear();
         }

         auto start = fc::time_point::now();
         for( uint32_t i = 0; i < cycles; ++i )
         {
            auto result = db->apply_transaction( transactions[i], ~0 );
            assets[i] = result.operation_results[0].get<object_id_result>().result;
         }
         auto end = fc::time_point::now();
         auto elapsed = end - start;
         total_time += elapsed.count();
         wlog( "${aps} asset create/s over ${total}ms",
               ("aps",(cycles*1000000)/elapsed.count())("total",elapsed.count()/1000) );
         trx.clear();
      }

      {
         asset_issue_operation aio;
         for( uint32_t i = 0; i < cycles; ++i )
         {
            aio.issuer = accounts[i];
            aio.issue_to_account = accounts[i+1];
            aio.asset_to_issue = asset( 10, assets[i] );
            trx.operations.push_back( aio );
            ++total_count;
            transactions[i] = trx;
            trx.operations.clear();
         }

         auto start = fc::time_point::now();
         for( uint32_t i = 0; i < cycles; ++i )
            db->apply_transaction( transactions[i], ~0 );
         auto end = fc::time_point::now();
         auto elapsed = end - start;
         total_time += elapsed.count();
         wlog( "${aps} issuances/s over ${total}ms",
               ("aps",(cycles*1000000)/elapsed.count())("total",elapsed.count()/1000) );
         trx.clear();
      }

      wlog( "${total} operations in ${total_time}ms => ${avg} ops/s on average",
            ("total",total_count)("total_time",total_time/1000)
            ("avg",(total_count*1000000)/total_time) );

      db->_undo_db.enable();
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()



//#define BOOST_TEST_MODULE "C++ Unit Tests for Graphene Blockchain Database"
#include <cstdlib>
#include <iostream>
#include <boost/test/included/unit_test.hpp>

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {
   std::srand(time(NULL));
   std::cout << "Random number generator seeded to " << time(NULL) << std::endl;
   return nullptr;
}
