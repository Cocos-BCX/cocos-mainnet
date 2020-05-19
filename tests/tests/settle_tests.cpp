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

#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/protocol/market.hpp>
#include <graphene/chain/market_object.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( settle_tests, database_fixture )

BOOST_AUTO_TEST_CASE( settle_rounding_test )
{
   try {
      // get around Graphene issue #615 feed expiration bug
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      set_expiration( db.get(), trx );

      ACTORS((paul)(michael)(rachel)(alice)(bob)(ted)(joe)(jim));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& bitcny = create_bitasset("CNYBIT", paul_id);
      const auto& core   = asset_id_type()(*db);
      asset_id_type bitusd_id = bitusd.id;
      asset_id_type bitcny_id = bitcny.id;
      asset_id_type core_id = core.id;

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id, asset(10000000));
      transfer(committee_account, alice_id, asset(10000000));
      transfer(committee_account, bob_id, asset(10000000));
      transfer(committee_account, jim_id, asset(10000000));

      // add a feed to asset
      update_feed_producers( bitusd, {paul.id} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(5);
      publish_feed( bitusd, paul, current_feed );

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul, bitusd.amount(1000), core.amount(100) );
      call_order_id_type call_paul_id = call_paul.id;
      BOOST_REQUIRE_EQUAL( get_balance( paul, bitusd ), 1000 );

      // and transfer some to rachel
      transfer(paul.id, rachel.id, asset(200, bitusd.id));

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 200);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 100000000);

      // michael gets some bitusd
      const call_order_object& call_michael = *borrow(michael, bitusd.amount(6), core.amount(8));
      call_order_id_type call_michael_id = call_michael.id;

      // add settle order and check rounding issue
      operation_result result = force_settle(rachel, bitusd.amount(4));

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 196);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 6);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul, core), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul, bitusd), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul.debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul.collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael.debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael.collateral.value );

      generate_blocks( db->head_block_time() + fc::hours(20) );
      set_expiration( db.get(), trx );

      // default feed and settlement expires at the same time
      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 100 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      // now yes expire settlement
      generate_blocks( db->head_block_time() + fc::hours(6) );

      // checks
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0); // rachel paid 4 usd and got nothing
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 196);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 800);

      BOOST_CHECK_EQUAL( 996, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 1002 ); // 1000 + 6 - 4

      // settle more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test less than it
      set_expiration( db.get(), trx );
      operation_result result2 = force_settle(rachel_id(*db), bitusd_id(*db).amount(34));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 162); // 196-34
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 800);

      BOOST_CHECK_EQUAL( 996, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).collateral.value );

      generate_blocks( db->head_block_time() + fc::hours(10) );
      set_expiration( db.get(), trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 100 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      // now yes expire settlement
      generate_blocks( db->head_block_time() + fc::hours(16) );
      set_expiration( db.get(), trx );

      // checks
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 1); // rachel got 1 core and paid 34 usd
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 162);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 800);

      BOOST_CHECK_EQUAL( 962, call_paul_id(*db).debt.value ); // 996 - 34
      BOOST_CHECK_EQUAL( 99, call_paul_id(*db).collateral.value ); // 100 - 1
      BOOST_CHECK_EQUAL( 6, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 968 ); // 1002 - 34

      // prepare for more tests
      transfer(paul_id, rachel_id, asset(300, bitusd_id));
      borrow(michael_id(*db), bitusd_id(*db).amount(2), core_id(*db).amount(3));

      // settle even more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test more than it
      const operation_result result3 = force_settle(rachel_id(*db), bitusd_id(*db).amount(3));
      const operation_result result4 = force_settle(rachel_id(*db), bitusd_id(*db).amount(434));
      const operation_result result5 = force_settle(rachel_id(*db), bitusd_id(*db).amount(5));


      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20); // 162 + 300 - 3 - 434 - 5
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 8); // 6 + 2
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999989); // 99999992 - 3
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500); // 800 - 300

      BOOST_CHECK_EQUAL( 962, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 99, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).debt.value ); // 6 + 2
      BOOST_CHECK_EQUAL( 11, call_michael_id(*db).collateral.value ); // 8 + 3

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 970 ); // 968 + 2

      generate_blocks( db->head_block_time() + fc::hours(4) );
      set_expiration( db.get(), trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // now yes expire settlement
      generate_blocks( db->head_block_time() + fc::hours(22) );
      set_expiration( db.get(), trx );

      // checks
      // maximum amount that can be settled now is round_down(970 * 20%) = 194.
      // settle_id3 (amount was 3) will be filled and get nothing.
      // settle_id4 will pay 194 - 3 = 191 usd, will get round_down(191*5/101) = 9 core
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10); // 1 + 9
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 8);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999989);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 768, call_paul_id(*db).debt.value ); // 962 - 3 - 191
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value ); // 99 - 9
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 11, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 776 ); // 970 - 3 - 191
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 194 ); // 3 + 191

      generate_block();

      // michael borrows more
      set_expiration( db.get(), trx );
      borrow(michael_id(*db), bitusd_id(*db).amount(18), core_id(*db).amount(200));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 26); // 8 + 18
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999789); // 99999989 - 200
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 768, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(*db).debt.value ); // 8 + 18
      BOOST_CHECK_EQUAL( 211, call_michael_id(*db).collateral.value ); // 11 + 200

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 794 ); // 776 + 18
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 194 );

      generate_block();

      // maximum amount that can be settled now is round_down((794+194) * 20%) = 197,
      //   already settled 194, so 197 - 194 = 3 more usd can be settled,
      //   so settle_id3 will pay 3 usd and get nothing

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 26);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999789);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 765, call_paul_id(*db).debt.value ); // 768 - 3
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 211, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 791 ); // 794 - 3
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 197 ); // 194 + 3

      // michael borrows a little more
      set_expiration( db.get(), trx );
      borrow(michael_id(*db), bitusd_id(*db).amount(20), core_id(*db).amount(20));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46); // 26 + 20
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769); // 99999789 - 20
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 765, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value ); // 26 + 20
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value ); // 211 + 20

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 811 ); // 791 + 20
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 197 );

      generate_block();

      // maximum amount that can be settled now is round_down((811+197) * 20%) = 201,
      //   already settled 197, so 201 - 197 = 4 more usd can be settled,
      //   so settle_id4 will pay 4 usd and get nothing
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 761, call_paul_id(*db).debt.value ); // 765 - 4
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 807 ); // 811 - 4
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 201 ); // 197 + 4

      generate_block();

      // jim borrow some cny
      call_order_id_type call_jim_id = borrow(jim_id(*db), bitcny_id(*db).amount(2000), core_id(*db).amount(2000))->id;

      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).collateral.value );

      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 2000);

      // jim transfer some cny to joe
      transfer(jim_id, joe_id, asset(1500, bitcny_id));

      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 1500);

      generate_block();

      // give ted some usd
      transfer(paul_id, ted_id, asset(100, bitusd_id));
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 100); // new: 100
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400); // 500 - 100

      // ted settle
      const operation_result result6 = force_settle(ted_id(*db), bitusd_id(*db).amount(20));
      const operation_result result7 = force_settle(ted_id(*db), bitusd_id(*db).amount(21));
      const operation_result result8 = force_settle(ted_id(*db), bitusd_id(*db).amount(22));


      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 37); // 100 - 20 - 21 - 22

      // joe settle
      const operation_result result101 = force_settle(joe_id(*db), bitcny_id(*db).amount(100));
      const operation_result result102 = force_settle(joe_id(*db), bitcny_id(*db).amount(1000));
      const operation_result result103 = force_settle(joe_id(*db), bitcny_id(*db).amount(300));


      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 100); // 1500 - 100 - 1000 - 300

      generate_block();

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // get to another maintenance interval
      generate_blocks( db->head_block_time() + fc::hours(22) );
      set_expiration( db.get(), trx );

      // maximum amount that can be settled now is round_down(807 * 20%) = 161,
      // settle_id4 will pay 161 usd, will get round_down(161*5/101) = 7 core
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 17); // 10 + 7
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 37);

      BOOST_CHECK_EQUAL( 600, call_paul_id(*db).debt.value ); // 761 - 161
      BOOST_CHECK_EQUAL( 83, call_paul_id(*db).collateral.value ); // 90 - 7
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 646 ); // 807 - 161
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 161 ); // reset to 0, then 161 more

      // current cny data
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 100); // 1500 - 100 - 1000 - 300

      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitcny_id(*db).dynamic_data(*db).current_supply.value, 2000 );
      BOOST_CHECK_EQUAL( bitcny_id(*db).bitasset_data(*db).force_settled_volume.value, 0 );

      // bob borrow some
      const call_order_object& call_bob = *borrow( bob_id(*db), bitusd_id(*db).amount(19), core_id(*db).amount(2) );
      call_order_id_type call_bob_id = call_bob.id;

      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), core_id(*db)), 9999998); // 10000000 - 2
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), bitusd_id(*db)), 19); // new

      BOOST_CHECK_EQUAL( 19, call_bob_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 2, call_bob_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 665 ); // 646 + 19
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 161 );

      generate_block();

      // maximum amount that can be settled now is round_down((665+161) * 20%) = 165,
      // settle_id4 will pay 165-161=4 usd, will get nothing
      // bob's call order will get partially settled since its collateral ratio is the lowest
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), core_id(*db)), 9999998);
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), bitusd_id(*db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 17); // no change
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 37);

      BOOST_CHECK_EQUAL( 15, call_bob_id(*db).debt.value ); // 19 - 4
      BOOST_CHECK_EQUAL( 2, call_bob_id(*db).collateral.value ); // no change
      BOOST_CHECK_EQUAL( 600, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 83, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 661 ); // 665 - 4
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 165 ); // 161 + 4

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // generate some blocks
      generate_blocks( db->head_block_time() + fc::hours(10) );
      set_expiration( db.get(), trx );

      // check cny
      // maximum amount that can be settled now is round_down(2000 * 20%) = 400,
      //   settle_id101's remaining amount is 100, so it can be fully processed,
      //      according to price 50 core / 101 cny, it will get 49 core and pay 100 cny;
      //   settle_id102's remaining amount is 1000, so 400-100=300 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 148 core and pay 300 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 197); // 49 + 148
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 100);

      BOOST_CHECK_EQUAL( 1600, call_jim_id(*db).debt.value ); // 2000 - 100 - 300
      BOOST_CHECK_EQUAL( 1803, call_jim_id(*db).collateral.value ); // 2000 - 49 - 148

      BOOST_CHECK_EQUAL( bitcny_id(*db).dynamic_data(*db).current_supply.value, 1600 );
      BOOST_CHECK_EQUAL( bitcny_id(*db).bitasset_data(*db).force_settled_volume.value, 400 ); // 100 + 300

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // get to another maintenance interval
      generate_blocks( db->head_block_time() + fc::hours(14) );
      set_expiration( db.get(), trx );

      // maximum amount that can be settled now is round_down(661 * 20%) = 132,
      //   settle_id4's remaining amount is 71,
      //      firstly it will pay 15 usd to call_bob and get nothing,
      //        call_bob will pay off all debt, so it will be closed and remaining collateral (2 core) will be returned;
      //      then it will pay 71-15=56 usd to call_paul and get round_down(56*5/101) = 2 core;
      //   settle_id5 (has 5 usd) will pay 5 usd and get nothing;
      //   settle_id6 (has 20 usd) will pay 20 usd and get nothing;
      //   settle_id7 (has 21 usd) will pay 21 usd and get 1 core;
      //   settle_id8 (has 22 usd) will pay 15 usd and get nothing, since reached 132


      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), core_id(*db)), 10000000); // 9999998 + 2
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), bitusd_id(*db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 19); // 17 + 2
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 20);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 1); // 0 + 1
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 37);

      BOOST_CHECK( !db->find( call_bob_id ) );
      BOOST_CHECK_EQUAL( 483, call_paul_id(*db).debt.value ); // 600 - 56 - 5 - 20 - 21 - 15
      BOOST_CHECK_EQUAL( 80, call_paul_id(*db).collateral.value ); // 83 - 2 - 1
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 529 ); // 661 - 132
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 132 ); // reset to 0, then 132 more

      // check cny
      // maximum amount that can be settled now is round_down(1600 * 20%) = 320,
      //   settle_id102's remaining amount is 700, so 320 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 158 core and pay 320 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 355); // 197 + 158
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 100);

      BOOST_CHECK_EQUAL( 1280, call_jim_id(*db).debt.value ); // 1600 - 320
      BOOST_CHECK_EQUAL( 1645, call_jim_id(*db).collateral.value ); // 1803 - 158

      BOOST_CHECK_EQUAL( bitcny_id(*db).dynamic_data(*db).current_supply.value, 1280 );
      BOOST_CHECK_EQUAL( bitcny_id(*db).bitasset_data(*db).force_settled_volume.value, 320 ); // reset to 0, then 320

      generate_block();

      // Note: the scenario that a big settle order matching several smaller call orders,
      //       and another scenario about force_settlement_offset_percent parameter,
      //       are tested in force_settle_test in operation_test2.cpp.

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( settle_rounding_test_after_hf_184 )
{
   try {
      auto mi = db->get_global_properties().parameters.maintenance_interval;
      
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      set_expiration( db.get(), trx );

      ACTORS((paul)(michael)(rachel)(alice)(bob)(ted)(joe)(jim));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& bitcny = create_bitasset("CNYBIT", paul_id);
      const auto& core   = asset_id_type()(*db);
      asset_id_type bitusd_id = bitusd.id;
      asset_id_type bitcny_id = bitcny.id;
      asset_id_type core_id = core.id;

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id, asset(10000000));
      transfer(committee_account, alice_id, asset(10000000));
      transfer(committee_account, bob_id, asset(10000000));
      transfer(committee_account, jim_id, asset(10000000));

      // add a feed to asset
      update_feed_producers( bitusd, {paul.id} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(5);
      publish_feed( bitusd, paul, current_feed );

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul, bitusd.amount(1000), core.amount(100) );
      call_order_id_type call_paul_id = call_paul.id;
      BOOST_REQUIRE_EQUAL( get_balance( paul, bitusd ), 1000 );

      // and transfer some to rachel
      transfer(paul.id, rachel.id, asset(200, bitusd.id));

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 200);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 100000000);

      // michael gets some bitusd
      const call_order_object& call_michael = *borrow(michael, bitusd.amount(6), core.amount(8));
      call_order_id_type call_michael_id = call_michael.id;

      // add settle order and check rounding issue
      const operation_result result = force_settle(rachel, bitusd.amount(4));

      BOOST_CHECK_EQUAL(get_balance(rachel, core), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel, bitusd), 196);
      BOOST_CHECK_EQUAL(get_balance(michael, bitusd), 6);
      BOOST_CHECK_EQUAL(get_balance(michael, core), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul, core), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul, bitusd), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul.debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul.collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael.debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael.collateral.value );

      generate_blocks( db->head_block_time() + fc::hours(20) );
      set_expiration( db.get(), trx );

      // default feed and settlement expires at the same time
      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      // now yes expire settlement
      generate_blocks( db->head_block_time() + fc::hours(6) );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 200); // rachel's settle order is cancelled and he get refunded
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 1006 ); // 1000 + 6

      // settle more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test less than it
      set_expiration( db.get(), trx );
      const operation_result result2 = force_settle(rachel_id(*db), bitusd_id(*db).amount(34));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 166); // 200-34
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 800);

      BOOST_CHECK_EQUAL( 1000, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 100, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 6, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).collateral.value );

      generate_blocks( db->head_block_time() + fc::hours(10) );
      set_expiration( db.get(), trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      // now yes expire settlement
      generate_blocks( db->head_block_time() + fc::hours(16) );
      set_expiration( db.get(), trx );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 1); // rachel got 1 core
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 179); // paid 21 usd since 1 core worths a little more than 20 usd
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999992);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 800);

      BOOST_CHECK_EQUAL( 979, call_paul_id(*db).debt.value ); // 1000 - 21
      BOOST_CHECK_EQUAL( 99, call_paul_id(*db).collateral.value ); // 100 - 1
      BOOST_CHECK_EQUAL( 6, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 985 ); // 1006 - 21

      // prepare for more tests
      transfer(paul_id, rachel_id, asset(300, bitusd_id));
      borrow(michael_id(*db), bitusd_id(*db).amount(2), core_id(*db).amount(3));

      // settle even more and check rounding issue
      // by default 20% of total supply can be settled per maintenance interval, here we test more than it
      const operation_result result3 = force_settle(rachel_id(*db), bitusd_id(*db).amount(3));
      const operation_result result4 = force_settle(rachel_id(*db), bitusd_id(*db).amount(434));
      const operation_result result5 = force_settle(rachel_id(*db), bitusd_id(*db).amount(5));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 37); // 179 + 300 - 3 - 434 - 5
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 8); // 6 + 2
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999989); // 99999992 - 3
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500); // 800 - 300

      BOOST_CHECK_EQUAL( 979, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 99, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).debt.value ); // 6 + 2
      BOOST_CHECK_EQUAL( 11, call_michael_id(*db).collateral.value ); // 8 + 3

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 987 ); // 985 + 2

      generate_blocks( db->head_block_time() + fc::hours(4) );
      set_expiration( db.get(), trx );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // now yes expire settlement
      generate_blocks( db->head_block_time() + fc::hours(22) );
      set_expiration( db.get(), trx );

      // checks
      // settle_id3 will be cancelled due to too small.
      // maximum amount that can be settled now is round_down(987 * 20%) = 197,
      //   according to price (101/5), the amount worths more than 9 core but less than 10 core, so 9 core will be settled,
      //   and 9 core worths 181.5 usd, so rachel will pay 182 usd and get 9 core
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10); // 1 + 9
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 40); // 37 + 3
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 8);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999989);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(*db).debt.value ); // 979 - 182
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value ); // 99 - 9
      BOOST_CHECK_EQUAL( 8, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 11, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 805 ); // 987 - 182
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 182 );

      generate_block();

      // michael borrows more
      set_expiration( db.get(), trx );
      borrow(michael_id(*db), bitusd_id(*db).amount(18), core_id(*db).amount(200));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 26); // 8 + 18
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999789); // 99999989 - 200
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(*db).debt.value ); // 8 + 18
      BOOST_CHECK_EQUAL( 211, call_michael_id(*db).collateral.value ); // 11 + 200

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 823 ); // 805 + 18
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 182 );

      generate_block();

      // maximum amount that can be settled now is round_down((823+182) * 20%) = 201,
      //   already settled 182, so 201 - 182 = 19 more usd can be settled,
      //   according to price (101/5), the amount worths less than 1 core,
      //   so nothing will happen.
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 26);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999789);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 26, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 211, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 823 );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 182 );

      // michael borrows a little more
      set_expiration( db.get(), trx );
      borrow(michael_id(*db), bitusd_id(*db).amount(20), core_id(*db).amount(20));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 10);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46); // 26 + 20
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769); // 99999789 - 20
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 797, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 90, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value ); // 26 + 20
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value ); // 211 + 20

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 843 ); // 823 + 20
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 182 );

      generate_block();

      // maximum amount that can be settled now is round_down((843+182) * 20%) = 205,
      //   already settled 182, so 205 - 182 = 23 more usd can be settled,
      //   according to price (101/5), the amount worths more than 1 core but less than 2 core,
      //   so settle order will fill 1 more core, since 1 core worth more than 20 usd but less than 21 usd,
      //   so rachel will pay 21 usd and get 1 core

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 11); // 10 + 1
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 40); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 500);

      BOOST_CHECK_EQUAL( 776, call_paul_id(*db).debt.value ); // 797 - 21
      BOOST_CHECK_EQUAL( 89, call_paul_id(*db).collateral.value ); // 90 - 1
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 822 ); // 843 - 21
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 203 ); // 182 + 21

      // jim borrow some cny
      call_order_id_type call_jim_id = borrow(jim_id(*db), bitcny_id(*db).amount(2000), core_id(*db).amount(2000))->id;

      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).collateral.value );

      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 2000);

      // jim transfer some cny to joe
      transfer(jim_id, joe_id, asset(1500, bitcny_id));

      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 1500);

      generate_block();

      // give ted some usd
      transfer(paul_id, ted_id, asset(100, bitusd_id));
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 100); // new: 100
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400); // 500 - 100

      // ted settle
      const operation_result result6 = force_settle(ted_id(*db), bitusd_id(*db).amount(20));
      const operation_result result7 = force_settle(ted_id(*db), bitusd_id(*db).amount(21));
      const operation_result result8 = force_settle(ted_id(*db), bitusd_id(*db).amount(22));

      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 37); // 100 - 20 - 21 - 22

      // joe settle
      const operation_result result101 = force_settle(joe_id(*db), bitcny_id(*db).amount(100));
      const operation_result result102 = force_settle(joe_id(*db), bitcny_id(*db).amount(1000));
      const operation_result result103 = force_settle(joe_id(*db), bitcny_id(*db).amount(300));

      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 100); // 1500 - 100 - 1000 - 300

      generate_block();

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // get to another maintenance interval
      generate_blocks( db->head_block_time() + fc::hours(22) );
      set_expiration( db.get(), trx );

      // maximum amount that can be settled now is round_down(822 * 20%) = 164,
      //   according to price (101/5), the amount worths more than 8 core but less than 9 core,
      //   so settle order will fill 8 more core, since 8 core worth more than 161 usd but less than 162 usd,
      //   so rachel will pay 162 usd and get 8 core
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 19); // 11 + 8
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 40); // no change
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 37);

      BOOST_CHECK_EQUAL( 614, call_paul_id(*db).debt.value ); // 776 - 162
      BOOST_CHECK_EQUAL( 81, call_paul_id(*db).collateral.value ); // 89 - 8
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 660 ); // 822 - 162
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 162 ); // reset to 0, then 162 more

      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 100); // 1500 - 100 - 1000 - 300

      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 2000, call_jim_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitcny_id(*db).dynamic_data(*db).current_supply.value, 2000 );
      BOOST_CHECK_EQUAL( bitcny_id(*db).bitasset_data(*db).force_settled_volume.value, 0 );

      // bob borrow some
      const call_order_object& call_bob = *borrow( bob_id(*db), bitusd_id(*db).amount(19), core_id(*db).amount(2) );
      call_order_id_type call_bob_id = call_bob.id;

      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), core_id(*db)), 9999998); // 10000000 - 2
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), bitusd_id(*db)), 19); // new

      BOOST_CHECK_EQUAL( 19, call_bob_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 2, call_bob_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 679 ); // 660 + 19
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 162 );

      generate_block();

      // maximum amount that can be settled now is round_down((679+162) * 20%) = 168,
      //   already settled 162, so 168 - 162 = 6 more usd can be settled,
      //   according to price (101/5), the amount worths less than 1 core,
      //   so nothing will happen.
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), core_id(*db)), 9999998);
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), bitusd_id(*db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 40);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 37);

      BOOST_CHECK_EQUAL( 19, call_bob_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 2, call_bob_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 614, call_paul_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 81, call_paul_id(*db).collateral.value );
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 679 );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 162 );

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // generate some blocks
      generate_blocks( db->head_block_time() + fc::hours(10) );
      set_expiration( db.get(), trx );

      // check cny
      // maximum amount that can be settled now is round_down(2000 * 20%) = 400,
      //   settle_id101's remaining amount is 100, so it can be fully processed,
      //      according to price 50 core / 101 cny, it will get 49 core and pay 99 cny, the rest (1 cny) will be refunded;
      //   settle_id102's remaining amount is 1000, so 400-99=301 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 149 core and pay 301 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 198); // 49 + 149
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 101); // 100 + 1

      BOOST_CHECK_EQUAL( 1600, call_jim_id(*db).debt.value ); // 2000 - 99 - 301
      BOOST_CHECK_EQUAL( 1802, call_jim_id(*db).collateral.value ); // 2000 - 49 - 149

      BOOST_CHECK_EQUAL( bitcny_id(*db).dynamic_data(*db).current_supply.value, 1600 );
      BOOST_CHECK_EQUAL( bitcny_id(*db).bitasset_data(*db).force_settled_volume.value, 400 ); // 99 + 301

      // adding new feed so we have valid price to exit
      update_feed_producers( bitusd_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 101 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), alice_id(*db), current_feed );

      update_feed_producers( bitcny_id(*db), {alice_id} );
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitcny_id(*db).amount( 101 ) / core_id(*db).amount(50);
      publish_feed( bitcny_id(*db), alice_id(*db), current_feed );

      // get to another maintenance interval
      generate_blocks( db->head_block_time() + fc::hours(14) );
      set_expiration( db.get(), trx );

      // maximum amount that can be settled now is round_down(679 * 20%) = 135,
      //   settle_id4's remaining amount is 69, so it can be fully processed:
      //     firstly call_bob will be matched, since it owes only 19 usd which worths less than 1 core,
      //       it will pay 1 core, and the rest (2-1=1 core) will be returned, short position will be closed;
      //     then call_paul will be matched,
      //       according to price (101/5), the amount (69-19=50 usd) worths more than 2 core but less than 3 core,
      //       so settle_id4 will get 2 more core, since 2 core worth more than 40 usd but less than 41 usd,
      //       call_rachel will pay 41 usd and get 2 core, the rest (50-41=9 usd) will be returned due to too small.
      //   settle_id5 (has 5 usd) will be cancelled due to too small;
      //   settle_id6 (has 20 usd) will be cancelled as well due to too small;
      //   settle_id7 (has 21 usd) will be filled and get 1 core, since it worths more than 1 core; but no more fund can be returned;
      //   settle_id8 (has 22 usd) will be filled and get 1 core, and 1 usd will be returned.
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), core_id(*db)), 9999999); // 9999998 + 1
      BOOST_CHECK_EQUAL(get_balance(bob_id(*db), bitusd_id(*db)), 19);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 22); // 19 + 1 + 2
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 54); // 40 + 9 + 5
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 46);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999769);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 400);
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), core_id(*db)), 2); // 0 + 1 + 1
      BOOST_CHECK_EQUAL(get_balance(ted_id(*db), bitusd_id(*db)), 58); // 37 + 20 + 1

      BOOST_CHECK( !db->find( call_bob_id ) );
      BOOST_CHECK_EQUAL( 531, call_paul_id(*db).debt.value ); // 614 - 41 - 21 - 21
      BOOST_CHECK_EQUAL( 77, call_paul_id(*db).collateral.value ); // 81 - 2 - 1 - 1
      BOOST_CHECK_EQUAL( 46, call_michael_id(*db).debt.value );
      BOOST_CHECK_EQUAL( 231, call_michael_id(*db).collateral.value );

      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 577 ); // 679 - 19 - 41 - 21 - 21
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).force_settled_volume.value, 102 ); // reset to 0, then 19 + 41 + 21 + 21

      // check cny
      // maximum amount that can be settled now is round_down(1600 * 20%) = 320,
      //   settle_id102's remaining amount is 699, so 320 cny will be processed,
      //      according to price 50 core / 101 cny, it will get 158 core and pay 320 cny;
      //   settle_id103 won't be processed since it's after settle_id102
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), core_id(*db)), 9998000);
      BOOST_CHECK_EQUAL(get_balance(jim_id(*db), bitcny_id(*db)), 500);
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), core_id(*db)), 356); // 198 + 158
      BOOST_CHECK_EQUAL(get_balance(joe_id(*db), bitcny_id(*db)), 101);

      BOOST_CHECK_EQUAL( 1280, call_jim_id(*db).debt.value ); // 1600 - 320
      BOOST_CHECK_EQUAL( 1644, call_jim_id(*db).collateral.value ); // 1802 - 158

      BOOST_CHECK_EQUAL( bitcny_id(*db).dynamic_data(*db).current_supply.value, 1280 );
      BOOST_CHECK_EQUAL( bitcny_id(*db).bitasset_data(*db).force_settled_volume.value, 320 ); // reset to 0, then 320

      generate_block();

      // Note: the scenario that a big settle order matching several smaller call orders,
      //       and another scenario about force_settlement_offset_percent parameter,
      //       are tested in force_settle_test in operation_test2.cpp.

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( global_settle_rounding_test )
{
   try {
      // get around Graphene issue #615 feed expiration bug
      generate_block();
      set_expiration( db.get(), trx );

      ACTORS((paul)(michael)(rachel)(alice));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& core   = asset_id_type()(*db);
      asset_id_type bitusd_id = bitusd.id;
      asset_id_type core_id = core.id;

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id,    asset(  10000000 ) );
      transfer(committee_account, alice_id,   asset(  10000000 ) );

      // allow global settle in bitusd
      asset_update_operation op;
      op.issuer = bitusd.issuer;
      op.asset_to_update = bitusd.id;
      op.new_options.issuer_permissions = global_settle;
      op.new_options.flags = bitusd.options.flags;
      op.new_options.core_exchange_rate = price( asset(1,bitusd_id), asset(1,core_id) );
      trx.operations.push_back(op);
      sign(trx, paul_private_key);
      PUSH_TX(db.get(), trx);
      generate_block();
      trx.clear();

      // add a feed to asset
      update_feed_producers( bitusd_id(*db), {paul_id} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 100 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), paul_id(*db), current_feed );

      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 10000000);

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul_id(*db), bitusd_id(*db).amount(1001), core_id(*db).amount(101));
      call_order_id_type call_paul_id = call_paul.id;
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(*db), bitusd_id(*db) ), 1001 );
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(*db), core_id(*db) ), 10000000-101);

      // and transfer some to rachel
      transfer(paul_id, rachel_id, asset(200, bitusd_id));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);

      // michael borrow some bitusd
      const call_order_object& call_michael = *borrow(michael_id(*db), bitusd_id(*db).amount(6), core_id(*db).amount(8));
      call_order_id_type call_michael_id = call_michael.id;

      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 100000000-8);

      // add global settle
      force_global_settle(bitusd_id(*db), bitusd_id(*db).amount(10) / core_id(*db).amount(1));
      generate_block();

      BOOST_CHECK( bitusd_id(*db).bitasset_data(*db).settlement_price
                   == price( bitusd_id(*db).amount(1007), core_id(*db).amount(100) ) );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).settlement_fund.value, 100 ); // 100 from paul, and 0 from michael
      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 1007 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 100000000); // michael paid nothing for 6 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900); // paul paid 100 core for 1001 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);

      // all call orders are gone after global settle
      BOOST_CHECK( !db->find_object(call_paul_id) );
      BOOST_CHECK( !db->find_object(call_michael_id) );

      // add settle order and check rounding issue
      force_settle(rachel_id(*db), bitusd_id(*db).amount(4));
      generate_block();

      BOOST_CHECK( bitusd_id(*db).bitasset_data(*db).settlement_price
                   == price( bitusd_id(*db).amount(1007), core_id(*db).amount(100) ) );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).settlement_fund.value, 100 ); // paid nothing
      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 1003 ); // settled 4 usd

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 196); // rachel paid 4 usd and got nothing
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);

      // rachel settle more than 1 core
      force_settle(rachel_id(*db), bitusd_id(*db).amount(13));
      generate_block();

      BOOST_CHECK( bitusd_id(*db).bitasset_data(*db).settlement_price
                   == price( bitusd_id(*db).amount(1007), core_id(*db).amount(100) ) );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).settlement_fund.value, 99 ); // paid 1 core
      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 990 ); // settled 13 usd

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 183); // rachel paid 13 usd and got 1 core
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999900);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( global_settle_rounding_test_after_hf_184 )
{
   try {
      auto mi = db->get_global_properties().parameters.maintenance_interval;
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      set_expiration( db.get(), trx );

      ACTORS((paul)(michael)(rachel)(alice));

      // create assets
      const auto& bitusd = create_bitasset("USDBIT", paul_id);
      const auto& core   = asset_id_type()(*db);
      asset_id_type bitusd_id = bitusd.id;
      asset_id_type core_id = core.id;

      // fund accounts
      transfer(committee_account, michael_id, asset( 100000000 ) );
      transfer(committee_account, paul_id,    asset(  10000000 ) );
      transfer(committee_account, alice_id,   asset(  10000000 ) );

      // allow global settle in bitusd
      asset_update_operation op;
      op.issuer = bitusd_id(*db).issuer;
      op.asset_to_update = bitusd_id;
      op.new_options.issuer_permissions = global_settle;
      op.new_options.flags = bitusd.options.flags;
      op.new_options.core_exchange_rate = price( asset(1,bitusd_id), asset(1,core_id) );
      trx.operations.push_back(op);
      sign(trx, paul_private_key);
      PUSH_TX(db.get(), trx);
      generate_block();
      trx.clear();

      // add a feed to asset
      update_feed_producers( bitusd_id(*db), {paul_id} );
      price_feed current_feed;
      current_feed.maintenance_collateral_ratio = 1750;
      current_feed.maximum_short_squeeze_ratio = 1100;
      current_feed.settlement_price = bitusd_id(*db).amount( 100 ) / core_id(*db).amount(5);
      publish_feed( bitusd_id(*db), paul_id(*db), current_feed );

      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 10000000);

      // paul gets some bitusd
      const call_order_object& call_paul = *borrow( paul_id(*db), bitusd_id(*db).amount(1001), core_id(*db).amount(101));
      call_order_id_type call_paul_id = call_paul.id;
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(*db), bitusd_id(*db) ), 1001 );
      BOOST_REQUIRE_EQUAL( get_balance( paul_id(*db), core_id(*db) ), 10000000-101);

      // and transfer some to rachel
      transfer(paul_id, rachel_id, asset(200, bitusd_id));

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 100000000);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);

      // michael borrow some bitusd
      const call_order_object& call_michael = *borrow(michael_id(*db), bitusd_id(*db).amount(6), core_id(*db).amount(8));
      call_order_id_type call_michael_id = call_michael.id;

      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 100000000-8);

      // add global settle
      force_global_settle(bitusd_id(*db), bitusd_id(*db).amount(10) / core_id(*db).amount(1));
      generate_block();

      BOOST_CHECK( bitusd_id(*db).bitasset_data(*db).settlement_price
                   == price( bitusd_id(*db).amount(1007), core_id(*db).amount(102) ) );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).settlement_fund.value, 102 ); // 101 from paul, and 1 from michael
      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 1007 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999999); // michael paid 1 core for 6 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999899); // paul paid 101 core for 1001 usd
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);

      // all call orders are gone after global settle
      BOOST_CHECK( !db->find_object(call_paul_id));
      BOOST_CHECK( !db->find_object(call_michael_id));

      // settle order will not execute after HF due to too small
      GRAPHENE_REQUIRE_THROW( force_settle(rachel_id(*db), bitusd_id(*db).amount(4)), fc::exception );

      generate_block();

      // balances unchanged
      BOOST_CHECK( bitusd_id(*db).bitasset_data(*db).settlement_price
                   == price( bitusd_id(*db).amount(1007), core_id(*db).amount(102) ) );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).settlement_fund.value, 102 );
      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 1007 );

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 0);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 200);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999999);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);

      // rachel settle more than 1 core
      force_settle(rachel_id(*db), bitusd_id(*db).amount(13));
      generate_block();

      BOOST_CHECK( bitusd_id(*db).bitasset_data(*db).settlement_price
                   == price( bitusd_id(*db).amount(1007), core_id(*db).amount(102) ) );
      BOOST_CHECK_EQUAL( bitusd_id(*db).bitasset_data(*db).settlement_fund.value, 101 ); // paid 1 core
      BOOST_CHECK_EQUAL( bitusd_id(*db).dynamic_data(*db).current_supply.value, 997 ); // settled 10 usd

      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), core_id(*db)), 1);
      BOOST_CHECK_EQUAL(get_balance(rachel_id(*db), bitusd_id(*db)), 190); // rachel paid 10 usd and got 1 core, 3 usd returned
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), bitusd_id(*db)), 6);
      BOOST_CHECK_EQUAL(get_balance(michael_id(*db), core_id(*db)), 99999999);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), core_id(*db)), 9999899);
      BOOST_CHECK_EQUAL(get_balance(paul_id(*db), bitusd_id(*db)), 801);


   } FC_LOG_AND_RETHROW()
}

/**
 * Test case to reproduce https://github.com/bitshares/bitshares-core/issues/1883.
 * When there is only one fill_order object in the ticker rolling buffer, it should only be rolled out once.
 */
BOOST_AUTO_TEST_CASE( global_settle_ticker_test )
{
   try {
      generate_block();

      const auto& meta_idx = db->get_index_type<simple_index<graphene::market_history::market_ticker_meta_object>>();
      const auto& ticker_idx = db->get_index_type<graphene::market_history::market_ticker_index>().indices();
      const auto& history_idx = db->get_index_type<graphene::market_history::history_index>().indices();

      BOOST_CHECK_EQUAL( meta_idx.size(), 0 );
      BOOST_CHECK_EQUAL( ticker_idx.size(), 0 );
      BOOST_CHECK_EQUAL( history_idx.size(), 0 );

      ACTORS((judge)(alice));

      const auto& pmark = create_prediction_market("PMARK", judge_id);
      const auto& core  = asset_id_type()(*db);

      int64_t init_balance(1000000);
      transfer(committee_account, judge_id, asset(init_balance));
      transfer(committee_account, alice_id, asset(init_balance));

      BOOST_TEST_MESSAGE( "Open position with equal collateral" );
      borrow( alice, pmark.amount(1000), asset(1000) );

      BOOST_TEST_MESSAGE( "Globally settling" );
      force_global_settle( pmark, pmark.amount(1) / core.amount(1) );

      generate_block();
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == false );

         BOOST_CHECK( tick.base_volume == 1000 );
         BOOST_CHECK( tick.quote_volume == 1000 );
      }

      generate_blocks( db->head_block_time() + 86000 ); // less than a day
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      // nothing changes
      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == false );

         BOOST_CHECK( tick.base_volume == 1000 );
         BOOST_CHECK( tick.quote_volume == 1000 );
      }

      generate_blocks( db->head_block_time() + 4000 ); // now more than 24 hours
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      // the history is rolled out, new 24h volume should be 0
      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == true ); // the order should be skipped on next roll

         BOOST_CHECK( tick.base_volume == 0 );
         BOOST_CHECK( tick.quote_volume == 0 );
      }

      generate_block();
      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      // nothing changes
      {
         BOOST_CHECK_EQUAL( meta_idx.size(), 1 );
         BOOST_CHECK_EQUAL( ticker_idx.size(), 1 );
         BOOST_CHECK_EQUAL( history_idx.size(), 1 );

         const auto& meta = *meta_idx.begin();
         const auto& tick = *ticker_idx.begin();
         const auto& hist = *history_idx.begin();

         BOOST_CHECK( meta.rolling_min_order_his_id == hist.id );
         BOOST_CHECK( meta.skip_min_order_his_id == true );

         BOOST_CHECK( tick.base_volume == 0 );
         BOOST_CHECK( tick.quote_volume == 0 );
      }


   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
