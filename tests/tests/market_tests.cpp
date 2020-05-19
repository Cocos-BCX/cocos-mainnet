/*
 * Copyright (c) 2017 Peter Conrad, and other contributors.
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

BOOST_FIXTURE_TEST_SUITE(market_tests, database_fixture)

/***
 * Reproduce bitshares-core issue #338 #343 #453 #606 #625 #649
 */
BOOST_AUTO_TEST_CASE(issue_338_etc)
{ try {
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);
   asset_id_type usd_id = bitusd.id;
   asset_id_type core_id = core.id;

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.id;
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call3_id = call3.id;
   transfer(borrower, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // adjust price feed to get call_order into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This order slightly below the call price will not be matched #606
   limit_order_id_type sell_low = create_sell_order(seller, bitusd.amount(7), core.amount(59))->id;
   // This order above the MSSP will not be matched
   limit_order_id_type sell_high = create_sell_order(seller, bitusd.amount(7), core.amount(78))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_med = create_sell_order(seller, bitusd.amount(7), core.amount(60))->id;

   cancel_limit_order( sell_med(*db) );
   cancel_limit_order( sell_high(*db) );
   cancel_limit_order( sell_low(*db) );

   // current implementation: an incoming limit order will be filled at the
   // requested price #338
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(7), core.amount(60)) );
   BOOST_CHECK_EQUAL( 993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 60, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 993, call.debt.value );
   BOOST_CHECK_EQUAL( 14940, call.collateral.value );

   limit_order_id_type buy_low = create_sell_order(buyer, asset(90), bitusd.amount(10))->id;
   // margin call takes precedence
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(7), core.amount(60)) );
   BOOST_CHECK_EQUAL( 986, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 120, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 986, call.debt.value );
   BOOST_CHECK_EQUAL( 14880, call.collateral.value );

   limit_order_id_type buy_med = create_sell_order(buyer, asset(105), bitusd.amount(10))->id;
   // margin call takes precedence
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(7), core.amount(70)) );
   BOOST_CHECK_EQUAL( 979, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 190, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 979, call.debt.value );
   BOOST_CHECK_EQUAL( 14810, call.collateral.value );

   limit_order_id_type buy_high = create_sell_order(buyer, asset(115), bitusd.amount(10))->id;
   // margin call still has precedence (!) #625
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(7), core.amount(77)) );
   BOOST_CHECK_EQUAL( 972, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 267, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 972, call.debt.value );
   BOOST_CHECK_EQUAL( 14733, call.collateral.value );

   cancel_limit_order( buy_high(*db) );
   cancel_limit_order( buy_med(*db) );
   cancel_limit_order( buy_low(*db) );

   // call with more usd
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700), core.amount(7700)) );
   BOOST_CHECK_EQUAL( 272, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 7967, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 272, call.debt.value );
   BOOST_CHECK_EQUAL( 7033, call.collateral.value );

   // at this moment, collateralization of call is 7033 / 272 = 25.8
   // collateralization of call2 is 15500 / 1000 = 15.5
   // collateralization of call3 is 16000 / 1000 = 16

   // call more, still matches with the first call order #343
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(10), core.amount(110)) );
   BOOST_CHECK_EQUAL( 262, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 8077, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 262, call.debt.value );
   BOOST_CHECK_EQUAL( 6923, call.collateral.value );

   // at this moment, collateralization of call is 6923 / 262 = 26.4
   // collateralization of call2 is 15500 / 1000 = 15.5
   // collateralization of call3 is 16000 / 1000 = 16

   // force settle
   force_settle( seller, bitusd.amount(10) );
   BOOST_CHECK_EQUAL( 252, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 8077, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 262, call.debt.value );
   BOOST_CHECK_EQUAL( 6923, call.collateral.value );

   // call2 get settled #343
   BOOST_CHECK_EQUAL( 252, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 8177, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 262, call_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 6923, call_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 990, call2_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 15400, call2_id(*db).collateral.value );

   set_expiration( db.get(), trx );
   update_feed_producers( usd_id(*db), {feedproducer_id} );

   // at this moment, collateralization of call is 8177 / 252 = 32.4
   // collateralization of call2 is 15400 / 990 = 15.5
   // collateralization of call3 is 16000 / 1000 = 16

   // adjust price feed to get call2 into black swan territory, but not the first call order
   current_feed.settlement_price = asset(1, usd_id) / asset(20, core_id);
   publish_feed( usd_id(*db), feedproducer_id(*db), current_feed );
   // settlement price = 1/20, mssp = 1/22

   // black swan event doesn't occur #649
   BOOST_CHECK( !usd_id(*db).bitasset_data(*db).has_settlement() );

   // generate a block
   generate_block();

   set_expiration( db.get(), trx );
   update_feed_producers( usd_id(*db), {feedproducer_id} );

   // adjust price feed back
   current_feed.settlement_price = asset(1, usd_id) / asset(10, core_id);
   publish_feed( usd_id(*db), feedproducer_id(*db), current_feed );
   // settlement price = 1/10, mssp = 1/11

   transfer(borrower2_id, seller_id, asset(1000, usd_id));
   transfer(borrower3_id, seller_id, asset(1000, usd_id));

   // This would match but is blocked by sell_low, it has an amount same as call's debt which will be full filled later
   sell_med = create_sell_order(seller_id(*db), asset(262, usd_id), asset(2620))->id; // 1/10
   // Another big order above sell_med, blocked
   limit_order_id_type sell_med2 = create_sell_order(seller_id(*db), asset(1200, usd_id), asset(12120))->id; // 1/10.1
   // Another small order above sell_med2, blocked
   limit_order_id_type sell_med3 = create_sell_order(seller_id(*db), asset(120, usd_id), asset(1224))->id; // 1/10.2

   // generate a block, sell_low will expire
   BOOST_TEST_MESSAGE( "Expire sell_low" );
   BOOST_CHECK( db->find<limit_order_object>( sell_low ) == nullptr );

   // #453 multiple order matching issue occurs
   BOOST_CHECK( db->find<limit_order_object>( sell_med ) == nullptr ); // sell_med get filled
   BOOST_CHECK( db->find<limit_order_object>( sell_med2 ) != nullptr ); // sell_med2 is still there
   BOOST_CHECK( db->find<limit_order_object>( sell_med3 ) == nullptr ); // sell_med3 get filled
   BOOST_CHECK( db->find<call_order_object>( call_id ) == nullptr ); // the first call order get filled
   BOOST_CHECK( db->find<call_order_object>( call2_id ) == nullptr ); // the second call order get filled
   BOOST_CHECK( db->find<call_order_object>( call3_id ) != nullptr ); // the third call order is still there


} FC_LOG_AND_RETHROW() }

/***
 * Fixed bitshares-core issue #338 #343 #606 #625 #649
 */
BOOST_AUTO_TEST_CASE(hardfork_core_338_test)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;

   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);
   asset_id_type usd_id = bitusd.id;
   asset_id_type core_id = core.id;

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.id;
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call3_id = call3.id;
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // adjust price feed to get call_order into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This sell order above MSSP will not be matched with a call
   BOOST_CHECK( create_sell_order(seller, bitusd.amount(7), core.amount(78)) != nullptr );

   BOOST_CHECK_EQUAL( 2993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // This buy order is too low will not be matched with a sell order
   limit_order_id_type buy_low = create_sell_order(buyer, asset(90), bitusd.amount(10))->id;
   // This buy order at MSSP will be matched only if no margin call (margin call takes precedence)
   limit_order_id_type buy_med = create_sell_order(buyer, asset(110), bitusd.amount(10))->id;
   // This buy order above MSSP will be matched with a sell order (limit order with better price takes precedence)
   limit_order_id_type buy_high = create_sell_order(buyer, asset(111), bitusd.amount(10))->id;

   BOOST_CHECK_EQUAL( 0, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 90 - 110 - 111, get_balance(buyer, core) );

   // This order slightly below the call price will be matched: #606 fixed
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700), core.amount(5900) ) );

   // firstly it will match with buy_high, at buy_high's price: #625 fixed
   BOOST_CHECK( !db->find<limit_order_object>( buy_high ) );
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_med )->for_sale.value, 110 );
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_low )->for_sale.value, 90 );

   // buy_high pays 111 CORE, receives 10 USD goes to buyer's balance
   BOOST_CHECK_EQUAL( 10, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 90 - 110 - 111, get_balance(buyer, core) );
   // sell order pays 10 USD, receives 111 CORE, remaining 690 USD for sale, still at price 7/59

   // then it will match with call, at mssp: 1/11 = 690/7590 : #338 fixed
   BOOST_CHECK_EQUAL( 2293, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 7701, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 310, call.debt.value );
   BOOST_CHECK_EQUAL( 7410, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );

   // call's call_price will be updated after the match, to 741/31/1.75 CORE/USD = 2964/217
   // it's above settlement price (10/1) so won't be margin called again

   // This would match with call before, but would match with call2 after #343 fixed
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700), core.amount(6000) ) );
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_med )->for_sale.value, 110 );
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_low )->for_sale.value, 90 );

   // fill price would be mssp: 1/11 = 700/7700 : #338 fixed
   BOOST_CHECK_EQUAL( 1593, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 15401, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 310, call.debt.value );
   BOOST_CHECK_EQUAL( 7410, call.collateral.value );
   BOOST_CHECK_EQUAL( 300, call2.debt.value );
   BOOST_CHECK_EQUAL( 7800, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   // call2's call_price will be updated after the match, to 78/3/1.75 CORE/USD = 312/21
   // it's above settlement price (10/1) so won't be margin called

   // at this moment, collateralization of call is 7410 / 310 = 23.9
   // collateralization of call2 is 7800 / 300 = 26
   // collateralization of call3 is 16000 / 1000 = 16

   // force settle
   force_settle( seller, bitusd.amount(10) );

   BOOST_CHECK_EQUAL( 1583, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 15401, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 310, call.debt.value );
   BOOST_CHECK_EQUAL( 7410, call.collateral.value );
   BOOST_CHECK_EQUAL( 300, call2.debt.value );
   BOOST_CHECK_EQUAL( 7800, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );

   // generate blocks to let the settle order execute (price feed will expire after it)
   generate_block();
   generate_blocks( db->head_block_time() + fc::hours(24) );

   // call3 get settled, at settlement price 1/10: #343 fixed
   BOOST_CHECK_EQUAL( 1583, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 15501, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 310, call_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 7410, call_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 300, call2_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 7800, call2_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 990, call3_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 15900, call3_id(*db).collateral.value );

   set_expiration( db.get(), trx );
   update_feed_producers( usd_id(*db), {feedproducer_id} );

   // at this moment, collateralization of call is 7410 / 310 = 23.9
   // collateralization of call2 is 7800 / 300 = 26
   // collateralization of call3 is 15900 / 990 = 16.06

   // adjust price feed to get call3 into black swan territory, but not the other call orders
   // Note: after hard fork, black swan should occur when callateralization < mssp, but not at < feed
   current_feed.settlement_price = asset(1, usd_id) / asset(16, core_id);
   publish_feed( usd_id(*db), feedproducer_id(*db), current_feed );
   // settlement price = 1/16, mssp = 10/176

   // black swan event will occur: #649 fixed
   BOOST_CHECK( usd_id(*db).bitasset_data(*db).has_settlement() );
   // short positions will be closed
   BOOST_CHECK( !db->find<call_order_object>( call_id ) );
   BOOST_CHECK( !db->find<call_order_object>( call2_id ) );
   BOOST_CHECK( !db->find<call_order_object>( call3_id ) );

   // generate a block
   generate_block();


} FC_LOG_AND_RETHROW() }

/***
 * Fixed bitshares-core issue #453: multiple limit order filling issue
 */
BOOST_AUTO_TEST_CASE(hardfork_core_453_test)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;

   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);
   asset_id_type usd_id = bitusd.id;

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.id;
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call3_id = call3.id;
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // no margin call so far

   // This order would match call when it's margin called, it has an amount same as call's debt which will be full filled later
   limit_order_id_type sell_med = create_sell_order(seller_id(*db), asset(1000, usd_id), asset(10000))->id; // 1/10
   // Another big order above sell_med, amount bigger than call2's debt
   limit_order_id_type sell_med2 = create_sell_order(seller_id(*db), asset(1200, usd_id), asset(12120))->id; // 1/10.1
   // Another small order above sell_med2
   limit_order_id_type sell_med3 = create_sell_order(seller_id(*db), asset(120, usd_id), asset(1224))->id; // 1/10.2

   // adjust price feed to get the call orders  into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // Fixed #453 multiple order matching issue occurs
   BOOST_CHECK( !db->find<limit_order_object>( sell_med ) ); // sell_med get filled
   BOOST_CHECK( !db->find<limit_order_object>( sell_med2 ) ); // sell_med2 get filled
   BOOST_CHECK( !db->find<limit_order_object>( sell_med3 ) ); // sell_med3 get filled
   BOOST_CHECK( !db->find<call_order_object>( call_id ) ); // the first call order get filled
   BOOST_CHECK( !db->find<call_order_object>( call2_id ) ); // the second call order get filled
   BOOST_CHECK( db->find<call_order_object>( call3_id ) ); // the third call order is still there

   // generate a block
   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * Tests (big) limit order matching logic after #625 got fixed
 */
BOOST_AUTO_TEST_CASE(hardfork_core_625_big_limit_order_test)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;

   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db.get(), trx );

   ACTORS((buyer)(buyer2)(buyer3)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, buyer2_id, asset(init_balance));
   transfer(committee_account, buyer3_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.id;
   // create yet another position with 500% collateral, call price is 25/1.75 CORE/USD = 100/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(25000));
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( init_balance - 25000, get_balance(borrower3, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower3, bitusd) );

   // adjust price feed to get call and call2 (but not call3) into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This sell order above MSSP will not be matched with a call
   limit_order_id_type sell_high = create_sell_order(seller, bitusd.amount(7), core.amount(78))->id;
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_high )->for_sale.value, 7 );

   BOOST_CHECK_EQUAL( 2993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // This buy order is too low will not be matched with a sell order
   limit_order_id_type buy_low = create_sell_order(buyer, asset(80), bitusd.amount(10))->id;
   // This buy order at MSSP will be matched only if no margin call (margin call takes precedence)
   limit_order_id_type buy_med = create_sell_order(buyer2, asset(11000), bitusd.amount(1000))->id;
   // This buy order above MSSP will be matched with a sell order (limit order with better price takes precedence)
   limit_order_id_type buy_high = create_sell_order(buyer3, asset(111), bitusd.amount(10))->id;

   BOOST_CHECK_EQUAL( 0, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 80, get_balance(buyer, core) );
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(buyer2, core) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // Create a big sell order slightly below the call price, will be matched with several orders
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700*4), core.amount(5900*4) ) );

   // firstly it will match with buy_high, at buy_high's price
   BOOST_CHECK( !db->find<limit_order_object>( buy_high ) );
   // buy_high pays 111 CORE, receives 10 USD goes to buyer3's balance
   BOOST_CHECK_EQUAL( 10, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // then it will match with call, at mssp: 1/11 = 1000/11000
   BOOST_CHECK( !db->find<call_order_object>( call_id ) );
   // call pays 11000 CORE, receives 1000 USD to cover borrower's position, remaining CORE goes to borrower's balance
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );

   // then it will match with call2, at mssp: 1/11 = 1000/11000
   BOOST_CHECK( !db->find<call_order_object>( call2_id ) );
   // call2 pays 11000 CORE, receives 1000 USD to cover borrower2's position, remaining CORE goes to borrower2's balance
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );

   // then it will match with buy_med, at buy_med's price. Since buy_med is too big, it's partially filled.
   // buy_med receives the remaining USD of sell order, minus market fees, goes to buyer2's balance
   BOOST_CHECK_EQUAL( 783, get_balance(buyer2, bitusd) ); // 700*4-10-1000-1000=790, minus 1% market fee 790*100/10000=7
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(buyer2, core) );
   // buy_med pays at 1/11 = 790/8690
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_med )->for_sale.value, 11000-8690 );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_low )->for_sale.value, 80 );

   // check seller balance
   BOOST_CHECK_EQUAL( 193, get_balance(seller, bitusd) ); // 3000 - 7 - 700*4
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) ); // 111 + 11000 + 11000 + 8690

   // Cancel buy_med
   cancel_limit_order( buy_med(*db) );
   BOOST_CHECK( !db->find<limit_order_object>( buy_med ) );
   BOOST_CHECK_EQUAL( 783, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 8690, get_balance(buyer2, core) );

   // Create another sell order slightly below the call price, won't fill
   limit_order_id_type sell_med = create_sell_order( seller, bitusd.amount(7), core.amount(59) )->id;
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_med )->for_sale.value, 7 );
   // check seller balance
   BOOST_CHECK_EQUAL( 193-7, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_low )->for_sale.value, 80 );

   // generate a block
   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * Fixed bitshares-core issue #453 #606: multiple order matching without black swan, multiple bitassets
 */
BOOST_AUTO_TEST_CASE(hard_fork_453_cross_test)
{ try { // create orders before hard fork, which will be matched on hard fork
   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& biteur = create_bitasset("EURBIT", feedproducer_id);
   const auto& bitcny = create_bitasset("CNYBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);
   asset_id_type usd_id = bitusd.id;
   asset_id_type eur_id = biteur.id;
   asset_id_type cny_id = bitcny.id;
   asset_id_type core_id = core.id;

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );
   update_feed_producers( biteur, {feedproducer.id} );
   update_feed_producers( bitcny, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   current_feed.settlement_price = biteur.amount( 1 ) / core.amount(5);
   publish_feed( biteur, feedproducer, current_feed );
   current_feed.settlement_price = bitcny.amount( 1 ) / core.amount(5);
   publish_feed( bitcny, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call_usd = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_usd_id = call_usd.id;
   const call_order_object& call_eur = *borrow( borrower, biteur.amount(1000), asset(15000));
   call_order_id_type call_eur_id = call_eur.id;
   const call_order_object& call_cny = *borrow( borrower, bitcny.amount(1000), asset(15000));
   call_order_id_type call_cny_id = call_cny.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call_usd2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call_usd2_id = call_usd2.id;
   const call_order_object& call_eur2 = *borrow( borrower2, biteur.amount(1000), asset(15500));
   call_order_id_type call_eur2_id = call_eur2.id;
   const call_order_object& call_cny2 = *borrow( borrower2, bitcny.amount(1000), asset(15500));
   call_order_id_type call_cny2_id = call_cny2.id;
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call_usd3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call_usd3_id = call_usd3.id;
   const call_order_object& call_eur3 = *borrow( borrower3, biteur.amount(1000), asset(16000));
   call_order_id_type call_eur3_id = call_eur3.id;
   const call_order_object& call_cny3 = *borrow( borrower3, bitcny.amount(1000), asset(16000));
   call_order_id_type call_cny3_id = call_cny3.id;
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));
   transfer(borrower, seller, biteur.amount(1000));
   transfer(borrower2, seller, biteur.amount(1000));
   transfer(borrower3, seller, biteur.amount(1000));
   transfer(borrower, seller, bitcny.amount(1000));
   transfer(borrower2, seller, bitcny.amount(1000));
   transfer(borrower3, seller, bitcny.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call_usd.debt.value );
   BOOST_CHECK_EQUAL( 15000, call_usd.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call_usd2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call_usd2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call_usd3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call_usd3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 1000, call_eur.debt.value );
   BOOST_CHECK_EQUAL( 15000, call_eur.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call_eur2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call_eur2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call_eur3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call_eur3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, biteur) );
   BOOST_CHECK_EQUAL( 1000, call_cny.debt.value );
   BOOST_CHECK_EQUAL( 15000, call_cny.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call_cny2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call_cny2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call_cny3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call_cny3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitcny) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // adjust price feed to get call_order into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   current_feed.settlement_price = biteur.amount( 1 ) / core.amount(10);
   publish_feed( biteur, feedproducer, current_feed );
   current_feed.settlement_price = bitcny.amount( 1 ) / core.amount(10);
   publish_feed( bitcny, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This order below the call price will not be matched before hard fork: 1/8 #606
   limit_order_id_type sell_usd_low = create_sell_order(seller, bitusd.amount(1000), core.amount(7000))->id;
   // This is a big order, price below the call price will not be matched before hard fork: 1007/9056 = 1/8 #606
   limit_order_id_type sell_usd_low2 = create_sell_order(seller, bitusd.amount(1007), core.amount(8056))->id;
   // This order above the MSSP will not be matched before hard fork
   limit_order_id_type sell_usd_high = create_sell_order(seller, bitusd.amount(7), core.amount(78))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_usd_med = create_sell_order(seller, bitusd.amount(700), core.amount(6400))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_usd_med2 = create_sell_order(seller, bitusd.amount(7), core.amount(65))->id;

   // This order below the call price will not be matched before hard fork: 1/8 #606
   limit_order_id_type sell_eur_low = create_sell_order(seller, biteur.amount(1000), core.amount(7000))->id;
   // This is a big order, price below the call price will not be matched before hard fork: 1007/9056 = 1/8 #606
   limit_order_id_type sell_eur_low2 = create_sell_order(seller, biteur.amount(1007), core.amount(8056))->id;
   // This order above the MSSP will not be matched before hard fork
   limit_order_id_type sell_eur_high = create_sell_order(seller, biteur.amount(7), core.amount(78))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_eur_med = create_sell_order(seller, biteur.amount(700), core.amount(6400))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_eur_med2 = create_sell_order(seller, biteur.amount(7), core.amount(65))->id;

   // This order below the call price will not be matched before hard fork: 1/8 #606
   limit_order_id_type sell_cny_low = create_sell_order(seller, bitcny.amount(1000), core.amount(7000))->id;
   // This is a big order, price below the call price will not be matched before hard fork: 1007/9056 = 1/8 #606
   limit_order_id_type sell_cny_low2 = create_sell_order(seller, bitcny.amount(1007), core.amount(8056))->id;
   // This order above the MSSP will not be matched before hard fork
   limit_order_id_type sell_cny_high = create_sell_order(seller, bitcny.amount(7), core.amount(78))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_cny_med = create_sell_order(seller, bitcny.amount(700), core.amount(6400))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_cny_med2 = create_sell_order(seller, bitcny.amount(7), core.amount(65))->id;

   BOOST_CHECK_EQUAL( 3000-1000-1007-7-700-7, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 3000-1000-1007-7-700-7, get_balance(seller_id, eur_id) );
   BOOST_CHECK_EQUAL( 3000-1000-1007-7-700-7, get_balance(seller_id, cny_id) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // generate a block to include operations above
   generate_block();
   // go over the hard fork, make sure feed doesn't expire
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   // sell_low and call should get matched first
   BOOST_CHECK( !db->find<limit_order_object>( sell_usd_low ) );
   BOOST_CHECK( !db->find<call_order_object>( call_usd_id ) );
   // sell_low2 and call2 should get matched
   BOOST_CHECK( !db->find<call_order_object>( call_usd2_id ) );
   // sell_low2 and call3 should get matched: fixed #453
   BOOST_CHECK( !db->find<limit_order_object>( sell_usd_low2 ) );
   // sell_med and call3 should get matched
   BOOST_CHECK( !db->find<limit_order_object>( sell_usd_med ) );
   // call3 now is not at margin call state, so sell_med2 won't get matched
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_usd_med2 )->for_sale.value, 7 );
   // sell_high should still be there, didn't match anything
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_usd_high )->for_sale.value, 7 );

   // sell_low and call should get matched first
   BOOST_CHECK( !db->find<limit_order_object>( sell_eur_low ) );
   BOOST_CHECK( !db->find<call_order_object>( call_eur_id ) );
   // sell_low2 and call2 should get matched
   BOOST_CHECK( !db->find<call_order_object>( call_eur2_id ) );
   // sell_low2 and call3 should get matched: fixed #453
   BOOST_CHECK( !db->find<limit_order_object>( sell_eur_low2 ) );
   // sell_med and call3 should get matched
   BOOST_CHECK( !db->find<limit_order_object>( sell_eur_med ) );
   // call3 now is not at margin call state, so sell_med2 won't get matched
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_eur_med2 )->for_sale.value, 7 );
   // sell_high should still be there, didn't match anything
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_eur_high )->for_sale.value, 7 );

   // sell_low and call should get matched first
   BOOST_CHECK( !db->find<limit_order_object>( sell_cny_low ) );
   BOOST_CHECK( !db->find<call_order_object>( call_cny_id ) );
   // sell_low2 and call2 should get matched
   BOOST_CHECK( !db->find<call_order_object>( call_cny2_id ) );
   // sell_low2 and call3 should get matched: fixed #453
   BOOST_CHECK( !db->find<limit_order_object>( sell_cny_low2 ) );
   // sell_med and call3 should get matched
   BOOST_CHECK( !db->find<limit_order_object>( sell_cny_med ) );
   // call3 now is not at margin call state, so sell_med2 won't get matched
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_cny_med2 )->for_sale.value, 7 );
   // sell_high should still be there, didn't match anything
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_cny_high )->for_sale.value, 7 );

   // all match price would be limit order price
   BOOST_CHECK_EQUAL( 3000-1000-1007-7-700-7, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 3000-1000-1007-7-700-7, get_balance(seller_id, eur_id) );
   BOOST_CHECK_EQUAL( 3000-1000-1007-7-700-7, get_balance(seller_id, cny_id) );
   BOOST_CHECK_EQUAL( (7000+8056+6400)*3, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 1000-7-700, call_usd3_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 16000-56-6400, call_usd3_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 1000-7-700, call_eur3_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 16000-56-6400, call_eur3_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 1000-7-700, call_cny3_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 16000-56-6400, call_cny3_id(*db).collateral.value );
   // call3's call_price should be updated: 9544/293/1.75 = 9544*4 / 293*7 = 38176/2051 CORE/USD
   BOOST_CHECK( price(asset(38176),asset(2051,usd_id)) == call_usd3_id(*db).call_price );
   BOOST_CHECK( price(asset(38176),asset(2051,eur_id)) == call_eur3_id(*db).call_price );
   BOOST_CHECK( price(asset(38176),asset(2051,cny_id)) == call_cny3_id(*db).call_price );

   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * Fixed bitshares-core issue #338 #453 #606: multiple order matching with black swan
 */
BOOST_AUTO_TEST_CASE(hard_fork_338_cross_test)
{ try { // create orders before hard fork, which will be matched on hard fork
   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(borrower4)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);
   asset_id_type usd_id = bitusd.id;
   asset_id_type core_id = core.id;

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   transfer(committee_account, borrower4_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.id;
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call3_id = call3.id;
   // create yet another position with 400% collateral, call price is 20/1.75 CORE/USD = 80/7
   const call_order_object& call4 = *borrow( borrower4, bitusd.amount(1000), asset(20000));
   call_order_id_type call4_id = call4.id;
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // adjust price feed to get call_order into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This order below the call price will not be matched before hard fork: 1/8 #606
   limit_order_id_type sell_low = create_sell_order(seller, bitusd.amount(1000), core.amount(7000))->id;
   // This is a big order, price below the call price will not be matched before hard fork: 1007/9056 = 1/8 #606
   limit_order_id_type sell_low2 = create_sell_order(seller, bitusd.amount(1007), core.amount(8056))->id;
   // This would match but is blocked by sell_low?! #606
   limit_order_id_type sell_med = create_sell_order(seller, bitusd.amount(7), core.amount(64))->id;

   // adjust price feed to get call_order into black swan territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(16);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/16, mssp = 10/176

   // due to sell_low, black swan won't occur
   BOOST_CHECK( !usd_id(*db).bitasset_data(*db).has_settlement() );

   BOOST_CHECK_EQUAL( 3000-1000-1007-7, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // generate a block to include operations above
   generate_block();
   // go over the hard fork, make sure feed doesn't expire
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   // sell_low and call should get matched first
   BOOST_CHECK( !db->find<limit_order_object>( sell_low ) );
   BOOST_CHECK( !db->find<call_order_object>( call_id ) );
   // sell_low2 and call2 should get matched
   BOOST_CHECK( !db->find<call_order_object>( call2_id ) );
   // sell_low2 and call3 should get matched: fixed #453
   BOOST_CHECK( !db->find<limit_order_object>( sell_low2 ) );
   // sell_med and call3 should get matched
   BOOST_CHECK( !db->find<limit_order_object>( sell_med ) );

   // at this moment,
   // collateralization of call3 is (16000-56-64) / (1000-7-7) = 15880/986 = 16.1, it's > 16 but < 17.6
   // although there is no sell order, it should trigger a black swan event right away,
   // because after hard fork new limit order won't trigger black swan event
   BOOST_CHECK( usd_id(*db).bitasset_data(*db).has_settlement() );
   BOOST_CHECK( !db->find<call_order_object>( call3_id ) );
   BOOST_CHECK( !db->find<call_order_object>( call4_id ) );

   // since 16.1 > 16, global settlement should at feed price 16/1
   // so settlement fund should be 986*16 + 1000*16
   BOOST_CHECK_EQUAL( 1986*16, usd_id(*db).bitasset_data(*db).settlement_fund.value );
   // global settlement price should be 16/1, since no rounding here
   BOOST_CHECK( price(asset(1,usd_id),asset(16) ) == usd_id(*db).bitasset_data(*db).settlement_price );

   BOOST_CHECK_EQUAL( 3000-1000-1007-7, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 7000+8056+64, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower3_id, usd_id) );
   BOOST_CHECK_EQUAL( init_balance-16000+15880-986*16, get_balance(borrower3_id, core_id) );
   BOOST_CHECK_EQUAL( 1000, get_balance(borrower4_id, usd_id) );
   BOOST_CHECK_EQUAL( init_balance-1000*16, get_balance(borrower4_id, core_id) );

   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * Fixed bitshares-core issue #649: Black swan detection fetch call order by call_price but not collateral ratio
 */
BOOST_AUTO_TEST_CASE(hard_fork_649_cross_test)
{ try { // create orders before hard fork, which will be matched on hard fork
   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(borrower4)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);
   asset_id_type usd_id = bitusd.id;
   asset_id_type core_id = core.id;

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   transfer(committee_account, borrower4_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.id;
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call3_id = call3.id;
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // adjust price feed to get call_order into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This would match with call at price 707/6464
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(707), core.amount(6464)) );
   BOOST_CHECK_EQUAL( 3000-707, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 6464, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 293, call.debt.value );
   BOOST_CHECK_EQUAL( 8536, call.collateral.value );

   // at this moment,
   // collateralization of call is 8536 / 293 = 29.1
   // collateralization of call2 is 15500 / 1000 = 15.5
   // collateralization of call3 is 16000 / 1000 = 16

   generate_block();
   set_expiration( db.get(), trx );
   update_feed_producers( usd_id(*db), {feedproducer_id} );

   // adjust price feed to get call_order into black swan territory
   current_feed.settlement_price = price(asset(1,usd_id) / asset(20));
   publish_feed( usd_id(*db), feedproducer_id(*db), current_feed );
   // settlement price = 1/20, mssp = 1/22

   // due to #649, black swan won't occur
   BOOST_CHECK( !usd_id(*db).bitasset_data(*db).has_settlement() );

   // generate a block to include operations above
   generate_block();
   BOOST_CHECK( !usd_id(*db).bitasset_data(*db).has_settlement() );
   // go over the hard fork, make sure feed doesn't expire
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   // a black swan event should occur
   BOOST_CHECK( usd_id(*db).bitasset_data(*db).has_settlement() );
   BOOST_CHECK( !db->find<call_order_object>( call_id ) );
   BOOST_CHECK( !db->find<call_order_object>( call2_id ) );
   BOOST_CHECK( !db->find<call_order_object>( call3_id ) );

   // since least collateral ratio 15.5 < 20, global settlement should execute at price = least collateral ratio 15.5/1
   // so settlement fund should be 15500 + 15500 + round_up(15.5 * 293)
   BOOST_CHECK_EQUAL( 15500*2 + (293 * 155 + 9) / 10, usd_id(*db).bitasset_data(*db).settlement_fund.value );
   // global settlement price should be settlement_fund/(2000+293), but not 15.5/1 due to rounding
   BOOST_CHECK( price(asset(2293,usd_id),asset(15500*2+(293*155+9)/10) ) == usd_id(*db).bitasset_data(*db).settlement_price );

   BOOST_CHECK_EQUAL( 3000-707, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 6464, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower_id, usd_id) );
   BOOST_CHECK_EQUAL( init_balance-6464-(293*155+9)/10, get_balance(borrower_id, core_id) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2_id, usd_id) );
   BOOST_CHECK_EQUAL( init_balance-15500, get_balance(borrower2_id, core_id) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower3_id, usd_id) );
   BOOST_CHECK_EQUAL( init_balance-15500, get_balance(borrower3_id, core_id) );

   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * Fixed bitshares-core issue #343: change sorting of call orders when matching against limit order
 */
BOOST_AUTO_TEST_CASE(hard_fork_343_cross_test)
{ try { // create orders before hard fork, which will be matched on hard fork
   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(borrower4)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);
   asset_id_type usd_id = bitusd.id;
   asset_id_type core_id = core.id;

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   transfer(committee_account, borrower4_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.id;
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.id;
   // create yet another position with 350% collateral, call price is 17.5/1.75 CORE/USD = 77/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(17500));
   call_order_id_type call3_id = call3.id;
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 17500, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // adjust price feed to get call_order into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This would match with call at price 700/6400
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700), core.amount(6400)) );
   BOOST_CHECK_EQUAL( 3000-700, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 6400, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 300, call.debt.value );
   BOOST_CHECK_EQUAL( 8600, call.collateral.value );

   // at this moment,
   // collateralization of call is 8600 / 300 = 28.67
   // collateralization of call2 is 15500 / 1000 = 15.5
   // collateralization of call3 is 17500 / 1000 = 17.5

   // generate a block to include operations above
   generate_block();
   // go over the hard fork, make sure feed doesn't expire
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db.get(), trx );

   // This will match with call2 at price 7/77 (#343 fixed)
   BOOST_CHECK( !create_sell_order(seller_id(*db), asset(7*50,usd_id), asset(65*50)) );
   BOOST_CHECK_EQUAL( 3000-700-7*50, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 6400+77*50, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 300, call_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 8600, call_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 1000-7*50, call2_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 15500-77*50, call2_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 17500, call3_id(*db).collateral.value );

   // at this moment,
   // collateralization of call is 8600 / 300 = 28.67
   // collateralization of call2 is 11650 / 650 = 17.9
   // collateralization of call3 is 17500 / 1000 = 17.5

   // This will match with call3 at price 7/77 (#343 fixed)
   BOOST_CHECK( !create_sell_order(seller_id(*db), asset(7,usd_id), asset(65)) );
   BOOST_CHECK_EQUAL( 3000-700-7*50-7, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 6400+77*50+77, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 300, call_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 8600, call_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 1000-7*50, call2_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 15500-77*50, call2_id(*db).collateral.value );
   BOOST_CHECK_EQUAL( 1000-7, call3_id(*db).debt.value );
   BOOST_CHECK_EQUAL( 17500-77, call3_id(*db).collateral.value );

   // at this moment,
   // collateralization of call is 8600 / 300 = 28.67
   // collateralization of call2 is 11650 / 650 = 17.9
   // collateralization of call3 is 17423 / 993 = 17.55

   // no more margin call now
   BOOST_CHECK( create_sell_order(seller_id(*db), asset(7,usd_id), asset(65)) );

   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * BSIP38 "target_collateral_ratio" test: matching a taker limit order with multiple maker call orders
 */
BOOST_AUTO_TEST_CASE(target_cr_test_limit_call)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;

   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db.get(), trx );

   ACTORS((buyer)(buyer2)(buyer3)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, buyer2_id, asset(init_balance));
   transfer(committee_account, buyer3_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );

   // create yet another position with 500% collateral, call price is 25/1.75 CORE/USD = 100/7, no tcr
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(25000));
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( init_balance - 25000, get_balance(borrower3, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower3, bitusd) );

   // adjust price feed to get call and call2 (but not call3) into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This sell order above MSSP will not be matched with a call
   limit_order_id_type sell_high = create_sell_order(seller, bitusd.amount(7), core.amount(78))->id;
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_high )->for_sale.value, 7 );

   BOOST_CHECK_EQUAL( 2993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // This buy order is too low will not be matched with a sell order
   limit_order_id_type buy_low = create_sell_order(buyer, asset(80), bitusd.amount(10))->id;
   // This buy order at MSSP will be matched only if no margin call (margin call takes precedence)
   limit_order_id_type buy_med = create_sell_order(buyer2, asset(33000), bitusd.amount(3000))->id;
   // This buy order above MSSP will be matched with a sell order (limit order with better price takes precedence)
   limit_order_id_type buy_high = create_sell_order(buyer3, asset(111), bitusd.amount(10))->id;

   BOOST_CHECK_EQUAL( 0, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 80, get_balance(buyer, core) );
   BOOST_CHECK_EQUAL( init_balance - 33000, get_balance(buyer2, core) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // call and call2's CR is quite high, and debt amount is quite a lot, assume neither of them will be completely filled
   price match_price( bitusd.amount(1) / core.amount(11) );

   // Create a big sell order slightly below the call price, will be matched with several orders
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700*4), core.amount(5900*4) ) );

   // firstly it will match with buy_high, at buy_high's price
   BOOST_CHECK( !db->find<limit_order_object>( buy_high ) );
   // buy_high pays 111 CORE, receives 10 USD goes to buyer3's balance
   BOOST_CHECK_EQUAL( 10, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // call will receive call_to_cover, pay 11*call_to_cover
   // borrower's balance doesn't change
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );

   // call2 will receive call2_to_cover, pay 11*call2_to_cover
   // borrower2's balance doesn't change
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );

   // then it will match with buy_med, at buy_med's price. Since buy_med is too big, it's partially filled.
   // buy_med receives the remaining USD of sell order, minus market fees, goes to buyer2's balance
   BOOST_CHECK_EQUAL( init_balance - 33000, get_balance(buyer2, core) );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_low )->for_sale.value, 80 );

   // check seller balance
   BOOST_CHECK_EQUAL( 193, get_balance(seller, bitusd) ); // 3000 - 7 - 700*4
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) ); // 111 + (700*4-10)*11

   // Cancel buy_med
   cancel_limit_order( buy_med(*db) );
   BOOST_CHECK( !db->find<limit_order_object>( buy_med ) );

   // Create another sell order slightly below the call price, won't fill
   limit_order_id_type sell_med = create_sell_order( seller, bitusd.amount(7), core.amount(59) )->id;
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( sell_med )->for_sale.value, 7 );
   // check seller balance
   BOOST_CHECK_EQUAL( 193-7, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db->find<limit_order_object>( buy_low )->for_sale.value, 80 );

   // generate a block
   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * BSIP38 "target_collateral_ratio" test: matching a maker limit order with multiple taker call orders
 */
BOOST_AUTO_TEST_CASE(target_cr_test_call_limit)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;

   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db.get(), trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7, tcr 170% is lower than 175%

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_increase_before1270)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.id;
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.id;

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with mcr only
   current_feed.maintenance_collateral_ratio = 2000;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000  );

   print_market(bitusd.symbol, core.symbol);

   // both calls are still there, no margin call, mcr bug
   BOOST_CHECK( db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_increase_after1270)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.id;
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.id;

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with mcr only
   current_feed.maintenance_collateral_ratio = 2000;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998900 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 999100  );

   print_market(bitusd.symbol, core.symbol);

   // b1 is margin called
   BOOST_CHECK( ! db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_decrease_before1270)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.id;
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.id;

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with the feed
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(150);
   publish_feed( bitusd, feedproducer, current_feed );

   // getting out of margin call territory with mcr change
   current_feed.maintenance_collateral_ratio = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998350 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 999650  );

   print_market(bitusd.symbol, core.symbol);

   // margin call at b1, mcr bug
   BOOST_CHECK( !db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_decrease_after1270)
{ try {

   auto mi = db->get_global_properties().parameters.maintenance_interval;
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   set_expiration( db.get(), trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(*db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.id} );

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.id;
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.id;

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with the feed
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(150);
   publish_feed( bitusd, feedproducer, current_feed );

   // getting out of margin call territory with mcr decrease
   current_feed.maintenance_collateral_ratio = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000  );

   print_market(bitusd.symbol, core.symbol);

   // both calls are there, no margin call, good
   BOOST_CHECK( db->find<call_order_object>( b1_id ) );
   BOOST_CHECK( db->find<call_order_object>( b2_id ) );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_cross1270)
{ try {

   INVOKE(mcr_bug_increase_before1270);

   auto mi = db->get_global_properties().parameters.maintenance_interval;

   const asset_object& core = get_asset(GRAPHENE_SYMBOL);
   const asset_object& bitusd = get_asset("USDBIT");
   const asset_id_type bitusd_id = bitusd.id;
   const account_object& feedproducer = get_account("feedproducer");

   // feed is expired
   auto mcr = (*bitusd_id(*db).bitasset_data_id)(*db).current_feed.maintenance_collateral_ratio;
   BOOST_CHECK_EQUAL(mcr, GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

   // make new feed
   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 2000;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   mcr = (*bitusd_id(*db).bitasset_data_id)(*db).current_feed.maintenance_collateral_ratio;
   BOOST_CHECK_EQUAL(mcr, 2000);

   // pass hardfork
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   // feed is still valid
   mcr = (*bitusd_id(*db).bitasset_data_id)(*db).current_feed.maintenance_collateral_ratio;
   BOOST_CHECK_EQUAL(mcr, 2000);

   // margin call is traded
   print_market(asset_id_type(1)(*db).symbol, asset_id_type()(*db).symbol);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(hardfork_core_338_test_after_hf1270)
{ try {
   INVOKE(hardfork_core_338_test);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(hardfork_core_453_test_after_hf1270)
{ try {
   INVOKE(hardfork_core_453_test);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(hardfork_core_625_big_limit_order_test_after_hf1270)
{ try {
   INVOKE(hardfork_core_625_big_limit_order_test);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(target_cr_test_limit_call_after_hf1270)
{ try {
   INVOKE(target_cr_test_limit_call);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(target_cr_test_call_limit_after_hf1270)
{ try {
   INVOKE(target_cr_test_call_limit);
} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_SUITE_END()
