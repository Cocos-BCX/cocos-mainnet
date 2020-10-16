/*
 * Copyright (c) 2018 Abit More, and contributors.
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

#include <graphene/app/api.hpp>
#include <graphene/chain/hardfork.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(network_broadcast_api_tests, database_fixture)

BOOST_AUTO_TEST_CASE( broadcast_transaction_with_callback_test ) {
   try {

      uint32_t called = 0;
      auto callback = [&]( const variant& v )
      {
         ++called;
      };

      fc::ecc::private_key cid_key = fc::ecc::private_key::regenerate( fc::digest("key") );
      const account_id_type cid_id = create_account( "cid", cid_key.get_public_key() ).id;
      fund( cid_id(*db) );

      auto nb_api = std::make_shared< graphene::app::network_broadcast_api >( app );

      set_expiration( db.get(), trx );
      transfer_operation trans;
      trans.from = cid_id;
      trans.to   = account_id_type();
      trans.amount = asset(1);
      trx.operations.push_back( trans );
      sign( trx, cid_key );

      nb_api->broadcast_transaction_with_callback( callback, trx );

      trx.clear();

      generate_block();

      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      BOOST_CHECK_EQUAL( called, 1u );

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( broadcast_transaction_too_large ) {
   try {

      fc::ecc::private_key cid_key = fc::ecc::private_key::regenerate( fc::digest("key") );
      const account_id_type cid_id = create_account( "cid", cid_key.get_public_key() ).id;
      fund( cid_id(*db) );

      auto nb_api = std::make_shared< graphene::app::network_broadcast_api >( app );

      set_expiration( db.get(), trx );
      transfer_operation trans;
      trans.from = cid_id;
      trans.to   = account_id_type();
      trans.amount = asset(1);
      for(int i = 0; i < 250; ++i )
         trx.operations.push_back( trans );
      sign( trx, cid_key );

      BOOST_CHECK_THROW( nb_api->broadcast_transaction( trx ), fc::exception );

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
