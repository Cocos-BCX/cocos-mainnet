/*
 * Copyright (c) 2018 oxarbitrage, and contributors.
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

#include <graphene/chain/asset_object.hpp>
#include <graphene/app/api.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;

BOOST_FIXTURE_TEST_SUITE(asset_api_tests, database_fixture)

BOOST_AUTO_TEST_CASE( asset_holders )
{
   auto local_db = app.chain_database();
   graphene::app::asset_api asset_api(*local_db);

   // create an asset and some accounts
   create_bitasset("USD", account_id_type());
   auto dan = create_account("dan");
   auto bob = create_account("bob");
   auto alice = create_account("alice");

   // send them some bts
   transfer(account_id_type()(*db), dan, asset(100));
   transfer(account_id_type()(*db), alice, asset(200));
   transfer(account_id_type()(*db), bob, asset(300));

   // make call
   const vector<account_asset_balance> holders = asset_api.get_asset_holders( asset_id_type(), 0, 100);
   BOOST_CHECK_EQUAL(holders.size(), 4u);

   // by now we can guarantee the order
   BOOST_CHECK(holders[0].name == "committee-account");
   BOOST_CHECK(holders[1].name == "bob");
   BOOST_CHECK(holders[2].name == "alice");
   BOOST_CHECK(holders[3].name == "dan");
}
BOOST_AUTO_TEST_CASE( api_limit_get_asset_holders )
{
   auto local_db = app.chain_database();
   graphene::app::asset_api asset_api(*local_db);

   // create an asset and some accounts
   create_bitasset("USD", account_id_type());
   auto dan = create_account("dan");
   auto bob = create_account("bob");
   auto alice = create_account("alice");

   // send them some bts
   transfer(account_id_type()(*db), dan, asset(100));
   transfer(account_id_type()(*db), alice, asset(200));
   transfer(account_id_type()(*db), bob, asset(300));

   // make call
   GRAPHENE_CHECK_THROW(asset_api.get_asset_holders(asset_id_type(), 0, 260), fc::exception);
   vector<account_asset_balance> holders = asset_api.get_asset_holders(asset_id_type(), 0, 210);
   BOOST_REQUIRE_EQUAL( holders.size(), 4u );
}

BOOST_AUTO_TEST_SUITE_END()
