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
#include <boost/assign/list_of.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/protocol/lua_types.hpp>
#include <graphene/market_history/market_history_plugin.hpp>
#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

#define UIA_TEST_SYMBOL "UIATEST"

BOOST_FIXTURE_TEST_SUITE( operation_tests, database_fixture )


BOOST_AUTO_TEST_CASE(boost_mlp_begin_test)
{
   //typedef boost::mpl::list<char,short,int,long> types;
   typedef boost::variant<char,int> my_types;
   typedef boost::mpl::begin<typename my_types::types>::type iter;
   BOOST_STATIC_ASSERT(boost::is_same< boost::mpl::deref<iter>::type,char >::value);

   BOOST_STATIC_ASSERT(boost::is_same<
                     boost::mpl::deref<
                                 boost::mpl::next<iter>::type
                           >::type,
                                 int
                           >::value
                           );

}

BOOST_AUTO_TEST_CASE( feed_limit_logic_test )
{
   try {
      asset usd(1000,asset_id_type(1));
      asset core(1000,asset_id_type(0));
      price_feed feed;
      feed.settlement_price = usd / core;

      // require 3x min collateral
      auto swanp = usd / core;
      auto callp = ~price::call_price( usd, core, 1750 );
      // 1:1 collateral
      //  wdump((callp.to_real())(callp));
      //  wdump((swanp.to_real())(swanp));
      FC_ASSERT( callp.to_real() > swanp.to_real() );

      /*
      wdump((feed.settlement_price.to_real()));
      wdump((feed.maintenance_price().to_real()));
      wdump((feed.max_short_squeeze_price().to_real()));

      BOOST_CHECK( usd * feed.settlement_price < usd * feed.maintenance_price() );
      BOOST_CHECK( usd * feed.maintenance_price() < usd * feed.max_short_squeeze_price() );
      */

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( call_order_update_test )
{
   try {
      ACTORS((zebras)(antelope));
      const auto& bitusd = create_bitasset("USDBIT", antelope.id);
      const auto& core   = asset_id_type()(*db);

      transfer(committee_account, zebras_id, asset(10000000));
      update_feed_producers( bitusd, {antelope.id} );

      price_feed current_feed;
      current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
      current_feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different default
      publish_feed( bitusd, antelope, current_feed );

      wdump((bitusd.bitasset_data(*db).current_feed.settlement_price));
      FC_ASSERT( bitusd.bitasset_data(*db).current_feed.settlement_price == current_feed.settlement_price );

      BOOST_TEST_MESSAGE( "attempting to borrow using 2x collateral at 1:1 price now that there is a valid order" );
      borrow( zebras, bitusd.amount(5000), asset(10000));
      BOOST_REQUIRE_EQUAL( get_balance( zebras, bitusd ), 5000 );
      BOOST_REQUIRE_EQUAL( get_balance( zebras, core ), 10000000 - 10000 );

      BOOST_TEST_MESSAGE( "covering 2500 usd and freeing 5000 core..." );
      cover( zebras, bitusd.amount(2500), asset(5000));
      BOOST_REQUIRE_EQUAL( get_balance( zebras, bitusd ), 2500 );
      BOOST_REQUIRE_EQUAL( get_balance( zebras, core ), 10000000 - 10000 + 5000  );

      BOOST_TEST_MESSAGE( "verifying that attempting to cover the full amount without claiming the collateral fails" );
      GRAPHENE_REQUIRE_THROW( cover( zebras, bitusd.amount(2500), core.amount(0)  ), fc::exception );

      cover( zebras, bitusd.amount(2500), core.amount(5000));

      BOOST_REQUIRE_EQUAL( get_balance( zebras, bitusd ), 0 );
      BOOST_REQUIRE_EQUAL( get_balance( zebras, core ), 10000000  );

      borrow( zebras, bitusd.amount(5000), asset(10000));
      BOOST_REQUIRE_EQUAL( get_balance( zebras, bitusd ), 5000 );
      BOOST_REQUIRE_EQUAL( get_balance( zebras, core ), 10000000 - 10000  );


      // test just increasing collateral
      BOOST_TEST_MESSAGE( "increasing collateral" );
      borrow( zebras, bitusd.amount(0), asset(10000));

      BOOST_REQUIRE_EQUAL( get_balance( zebras, bitusd ), 5000 );
      BOOST_REQUIRE_EQUAL( get_balance( zebras, core ), 10000000 - 20000  );

      // test just decreasing debt
      BOOST_TEST_MESSAGE( "decreasing debt" );
      cover( zebras, bitusd.amount(1000), asset(0));

      BOOST_REQUIRE_EQUAL( get_balance( zebras, bitusd ), 4000 );
      BOOST_REQUIRE_EQUAL( get_balance( zebras, core ), 10000000 - 20000  );

      BOOST_TEST_MESSAGE( "increasing debt without increasing collateral" );
      borrow( zebras, bitusd.amount(1000), asset(0));

      BOOST_REQUIRE_EQUAL( get_balance( zebras, bitusd ), 5000 );
      BOOST_REQUIRE_EQUAL( get_balance( zebras, core ), 10000000 - 20000  );

      BOOST_TEST_MESSAGE( "increasing debt without increasing collateral again" );
      GRAPHENE_REQUIRE_THROW( borrow( zebras, bitusd.amount(80000), asset(0)), fc::exception );
      BOOST_TEST_MESSAGE( "attempting to claim all collateral without paying off debt" );
      GRAPHENE_REQUIRE_THROW( cover( zebras, bitusd.amount(0), asset(20000)), fc::exception );
      BOOST_TEST_MESSAGE( "attempting reduce collateral without paying off any debt" );
      cover( zebras, bitusd.amount(0), asset(1000));

      BOOST_TEST_MESSAGE( "attempting change call price to be below minimum for debt/collateral ratio" );
      GRAPHENE_REQUIRE_THROW( cover( zebras, bitusd.amount(0), asset(0)), fc::exception );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 *  This test sets up a situation where a margin call will be executed and ensures that
 *  it is properly filled.
 *
 *  A margin call can happen in the following situation:
 *  0. there exists a bid above the mas short squeeze price
 *  1. highest bid is lower than the call price of an order
 *  2. the asset is not a prediction market
 *  3. there is a valid price feed
 *
 *  This test creates two scenarios:
 *  a) when the bids are above the short squeese limit (should execute)
 *  b) when the bids are below the short squeeze limit (should not execute)
 */
/*
98843ms th_a       operation_tests.cpp:254       test_method          ] e.to_detail_string(): 13 N5boost12bad_rationalE: bad rational: zero denominator
bad rational: zero denominator:
    {"*this":{"settlement_price":{"base":{"amount":0,"asset_id":"1.3.0"},"quote":{"amount":0,"asset_id":"1.3.0"}},"maintenance_collateral_ratio":1750,"maximum_short_squeeze_ratio":1500},"what":"bad rational: zero denominator"}
    th_a  asset.cpp:152 validate
*/
BOOST_AUTO_TEST_CASE( margin_call_limit_test )
{
   try {
      ACTORS((buyer)(seller)(borrower)(borrower2)(feedproducer));

      const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
      const auto& core   = asset_id_type()(*db);

      int64_t init_balance(1000000);

      transfer(committee_account, buyer_id, asset(init_balance));
      transfer(committee_account, borrower_id, asset(init_balance));
      transfer(committee_account, borrower2_id, asset(init_balance));
      update_feed_producers( bitusd, {feedproducer.id} );

      price_feed current_feed;
      current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
      current_feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different default
      current_feed.maximum_short_squeeze_ratio  = 1500; // need to set this explicitly, testnet has a different default

      // starting out with price 1:1
      publish_feed( bitusd, feedproducer, current_feed );

      // start out with 2:1 collateral
      borrow( borrower, bitusd.amount(1000), asset(2000));
      borrow( borrower2, bitusd.amount(1000), asset(4000) );

      BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
      BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
      BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 2000 );
      BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 4000 );

      // this should trigger margin call that is below the call limit, but above the
      // protection threshold.
      BOOST_TEST_MESSAGE( "Creating a margin call that is NOT protected by the max short squeeze price" );
      auto order = create_sell_order( borrower2, bitusd.amount(1000), core.amount(1400) );
      /*
      if( db->head_block_time() <= HARDFORK_436_TIME )
      {
         BOOST_CHECK( order == nullptr );

         BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 4000 + 1400 );
         BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );

         BOOST_CHECK_EQUAL( get_balance( borrower, core ), init_balance - 2000 + 600 );
         BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
      }
      else*/
      {
         BOOST_CHECK( order != nullptr );

         BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
         BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
         BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 2000 );
         BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 4000 );
      }

      BOOST_TEST_MESSAGE( "Creating a margin call that is protected by the max short squeeze price" );
      borrow( borrower, bitusd.amount(1000), asset(2000) );
      borrow( borrower2, bitusd.amount(1000), asset(4000) );

      // this should trigger margin call without protection from the price feed.
      order = create_sell_order( borrower2, bitusd.amount(1000), core.amount(1800) );
      BOOST_CHECK( order != nullptr );
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

// BOOST_AUTO_TEST_CASE( prediction_market )
// {
//    try {
//       ACTORS((judge)(zebras)(nathan));

//       const auto& pmark = create_prediction_market("PMARK", judge_id);
//       const auto pmark_dd_id = pmark.dynamic_asset_data_id;
//       const auto& core  = asset_id_type()(*db);

//       int64_t init_balance(1000000);
//       transfer(committee_account, judge_id, asset(init_balance));
//       transfer(committee_account, zebras_id, asset(init_balance));
//       transfer(committee_account, nathan_id, asset(init_balance));

//       update_feed_producers( pmark, { judge_id });
//       price_feed feed;
//       feed.settlement_price = asset( 1, pmark.id ) / asset( 1 );
//       publish_feed( pmark, judge, feed );

//       BOOST_TEST_MESSAGE( "Require throw for mismatch collateral amounts" );
//       // GRAPHENE_REQUIRE_THROW( borrow( zebras, pmark.amount(1000), asset(2000) ), fc::exception );

//       // BOOST_TEST_MESSAGE( "Open position with equal collateral" );
//       // borrow( zebras, pmark.amount(1000), asset(1000) );

//       // BOOST_TEST_MESSAGE( "Cover position with unequal asset should fail." );
//       // GRAPHENE_REQUIRE_THROW( cover( zebras, pmark.amount(500), asset(1000) ), fc::exception );

//       // BOOST_TEST_MESSAGE( "Cover half of position with equal ammounts" );
//       // cover( zebras, pmark.amount(500), asset(500) );

//       // BOOST_TEST_MESSAGE( "Verify that forced settlment fails before global settlement" );
//       // GRAPHENE_REQUIRE_THROW( force_settle( zebras, pmark.amount(100) ), fc::exception );

//       // BOOST_TEST_MESSAGE( "Shouldn't be allowed to force settle at more than 1 collateral per debt" );
//       // GRAPHENE_REQUIRE_THROW( force_global_settle( pmark, pmark.amount(100) / core.amount(105) ), fc::exception );

//       // force_global_settle( pmark, pmark.amount(100) / core.amount(95) );

//       // BOOST_TEST_MESSAGE( "Verify that forced settlment succeedes after global settlement" );
//       // force_settle( zebras, pmark.amount(100) );

//       // // force settle the rest
//       // force_settle( zebras, pmark.amount(400) );
//       // BOOST_CHECK_EQUAL( 0, pmark_dd_id(*db).current_supply.value );

//       generate_block(~database::skip_transaction_dupe_check);
//       generate_blocks( db->get_dynamic_global_properties().next_maintenance_time );
//       generate_block();
//    } catch( const fc::exception& e) {
//       edump((e.to_detail_string()));
//       throw;
//    }
// }

// BOOST_AUTO_TEST_CASE( prediction_market_resolves_to_0 )
// {
//    try {
//       ACTORS((judge)(zebras)(nathan));

//       const auto& pmark = create_prediction_market("PMARK", judge_id);
//       const auto pmark_dd_id = pmark.dynamic_asset_data_id;
//       const auto& core  = asset_id_type()(*db);

//       int64_t init_balance(1000000);
//       transfer(committee_account, judge_id, asset(init_balance));
//       transfer(committee_account, zebras_id, asset(init_balance));
//       transfer(committee_account, nathan_id, asset(init_balance));

//       update_feed_producers( pmark, { judge_id });
//       price_feed feed;
//       feed.settlement_price = asset( 1, pmark.id ) / asset( 1 );
//       publish_feed( pmark, judge, feed );

//       borrow( zebras, pmark.amount(1000), asset(1000) );
//       // force settle with 0 outcome
//       force_global_settle( pmark, pmark.amount(100) / core.amount(0) );

//       BOOST_TEST_MESSAGE( "Verify that forced settlment succeedes after global settlement" );
//       force_settle( zebras, pmark.amount(100) );

//       // force settle the rest
//       force_settle( zebras, pmark.amount(900) );
//       BOOST_CHECK_EQUAL( 0, pmark_dd_id(*db).current_supply.value );

//       generate_block(~database::skip_transaction_dupe_check);
//       generate_blocks( db->get_dynamic_global_properties().next_maintenance_time );
//       generate_block();
// } catch( const fc::exception& e) {
//       edump((e.to_detail_string()));
//       throw;
//    }
// }

BOOST_AUTO_TEST_CASE( create_account_test )
{
   try {
      //generate_blocks( HARDFORK_CORE_143_TIME );
      generate_blocks( fc::time_point_sec( 1512747600 ) );
      set_expiration( db.get(), trx );
      trx.operations.push_back(make_account());
      account_create_operation op = trx.operations.back().get<account_create_operation>();

      REQUIRE_THROW_WITH_VALUE(op, registrar, account_id_type(9999999));
      // REQUIRE_THROW_WITH_VALUE(op, name, "!");
      // REQUIRE_THROW_WITH_VALUE(op, name, "antelope");
      // REQUIRE_THROW_WITH_VALUE(op, name, "antelope");
      // REQUIRE_THROW_WITH_VALUE(op, name, "antelope");
      // REQUIRE_THROW_WITH_VALUE(op, name, "6j");
      // REQUIRE_THROW_WITH_VALUE(op, name, "j-");
      // REQUIRE_THROW_WITH_VALUE(op, name, "-j");
      // REQUIRE_THROW_WITH_VALUE(op, name, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
      // REQUIRE_THROW_WITH_VALUE(op, name, "aaaa.");
      // REQUIRE_THROW_WITH_VALUE(op, name, ".aaaa");

      auto auth_bak = op.owner;
      op.owner.add_authority(account_id_type(9999999999), 10);
      trx.operations.back() = op;
      op.owner = auth_bak;
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx, ~0 ), fc::exception);
      op.owner = auth_bak;

      trx.operations.back() = op;
      sign( trx,  init_account_priv_key );
      trx.validate();
      PUSH_TX( db.get(), trx, ~0 );

      const account_object& nathan_account = *db->get_index_type<account_index>().indices().get<by_name>().find("nathan");
      BOOST_CHECK(nathan_account.id.space() == protocol_ids);
      BOOST_CHECK(nathan_account.id.type() == account_object_type);
      BOOST_CHECK(nathan_account.name == "nathan");

      BOOST_REQUIRE(nathan_account.owner.num_auths() == 1);
      BOOST_CHECK(nathan_account.owner.key_auths.at(committee_key) == 123);
      BOOST_REQUIRE(nathan_account.active.num_auths() == 1);
      BOOST_CHECK(nathan_account.active.key_auths.at(committee_key) == 321);
      BOOST_CHECK(nathan_account.options.memo_key == committee_key);

      const account_statistics_object& statistics = nathan_account.statistics(*db);
      BOOST_CHECK(statistics.id.space() == implementation_ids);
      BOOST_CHECK(statistics.id.type() == impl_account_statistics_object_type);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_account )
{
   try {
      const account_object& nathan = create_account("nathan", init_account_pub_key);
      const fc::ecc::private_key nathan_new_key = fc::ecc::private_key::generate();
      const public_key_type key_id = nathan_new_key.get_public_key();
      const auto& active_committee_members = db->get_global_properties().active_committee_members;

      transfer(account_id_type()(*db), nathan, asset(1000000000));

      trx.operations.clear();
      account_update_operation op;
      op.account = nathan.id;
      op.owner = authority(2, key_id, 1, init_account_pub_key, 1);
      op.active = authority(2, key_id, 1, init_account_pub_key, 1);
      op.new_options = nathan.options;
      op.new_options->votes = flat_set<vote_id_type>({active_committee_members[0](*db).vote_id, active_committee_members[5](*db).vote_id});
      // op.new_options->num_committee = 2;
      trx.operations.push_back(op);
      BOOST_TEST_MESSAGE( "Updating account" );
      PUSH_TX( db.get(), trx, ~0 );

      BOOST_CHECK(nathan.options.memo_key == init_account_pub_key);
      BOOST_CHECK(nathan.active.weight_threshold == 2);
      BOOST_CHECK(nathan.active.num_auths() == 2);
      BOOST_CHECK(nathan.active.key_auths.at(key_id) == 1);
      BOOST_CHECK(nathan.active.key_auths.at(init_account_pub_key) == 1);
      BOOST_CHECK(nathan.owner.weight_threshold == 2);
      BOOST_CHECK(nathan.owner.num_auths() == 2);
      BOOST_CHECK(nathan.owner.key_auths.at(key_id) == 1);
      BOOST_CHECK(nathan.owner.key_auths.at(init_account_pub_key) == 1);
      BOOST_CHECK(nathan.options.votes.size() == 2);

      enable_fees();
      {
         account_upgrade_operation op;
         op.account_to_upgrade = nathan.id;
         op.upgrade_to_lifetime_member = true;
         // op.fee = db->get_global_properties().parameters.current_fees->calculate_fee(op);
         trx.operations = {op};
         PUSH_TX( db.get(), trx, ~0 );
      }

      BOOST_CHECK( nathan.is_lifetime_member() );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( transfer_core_asset )
{
   try {
      INVOKE(create_account_test);

      account_id_type committee_account;
      asset committee_balance = db->get_balance(account_id_type(), asset_id_type());

      const account_object& nathan_account = *db->get_index_type<account_index>().indices().get<by_name>().find("nathan");
      transfer_operation top;
      top.from = committee_account;
      top.to = nathan_account.id;
      top.amount = asset( 10000);
      trx.operations.push_back(top);

      trx.validate();
      PUSH_TX( db.get(), trx, ~0 );

      committee_balance = db->get_balance(account_id_type(), asset_id_type());

      BOOST_CHECK_EQUAL(get_balance(nathan_account, asset_id_type()(*db)), 10000);

      trx = signed_transaction();
      top.from = nathan_account.id;
      top.to = committee_account;
      top.amount = asset(2000);
      trx.operations.push_back(top);

      set_expiration( db.get(), trx );
      trx.validate();
      PUSH_TX( db.get(), trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(account_id_type()(*db), asset_id_type()(*db)), committee_balance.amount.value + 2000);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_committee_member )
{
   try {
      committee_member_create_operation op;
      op.committee_member_account = account_id_type();
      // op.fee = asset();
      trx.operations.push_back(op);

      REQUIRE_THROW_WITH_VALUE(op, committee_member_account, account_id_type(99999999));
      // REQUIRE_THROW_WITH_VALUE(op, fee, asset(-600));
      trx.operations.back() = op;

      committee_member_id_type committee_member_id = db->get_index_type<primary_index<simple_index<committee_member_object>>>().get_next_id();
      PUSH_TX( db.get(), trx, ~0 );
      const committee_member_object& d = committee_member_id(*db);

      BOOST_CHECK(d.committee_member_account == account_id_type());
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_mia )
{
   try {
      const asset_object& bitusd = create_bitasset( "USDBIT" );
      BOOST_CHECK(bitusd.symbol == "USDBIT");
      BOOST_CHECK(bitusd.bitasset_data(*db).options.short_backing_asset == asset_id_type());
      BOOST_CHECK(bitusd.dynamic_asset_data_id(*db).current_supply == 0);
      GRAPHENE_REQUIRE_THROW( create_bitasset("USDBIT"), fc::exception);
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_mia )
{
   try {
      INVOKE(create_mia);
      generate_block();
      const asset_object& bit_usd = get_asset("USDBIT");

      asset_update_operation op;
      op.issuer = bit_usd.issuer;
      op.asset_to_update = bit_usd.id;
      op.new_options = bit_usd.options;
      trx.operations.emplace_back(op);

      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );
      std::swap(op.new_options.flags, op.new_options.issuer_permissions);
      op.new_issuer = account_id_type();
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );

      // {
      //    asset_publish_feed_operation pop;
      //    pop.asset_id = bit_usd.get_id();
      //    pop.publisher = get_account("init0").get_id();
      //    price_feed feed;
      //    feed.settlement_price = price(bit_usd.amount(5), bit_usd.amount(5));
      //    REQUIRE_THROW_WITH_VALUE(pop, feed, feed);
      //    feed.settlement_price = ~price(bit_usd.amount(5), asset(5));
      //    REQUIRE_THROW_WITH_VALUE(pop, feed, feed);
      //    feed.settlement_price = price(bit_usd.amount(5), asset(5));
      //    pop.feed = feed;
      //    REQUIRE_THROW_WITH_VALUE(pop, feed.maintenance_collateral_ratio, 0);
      //    trx.operations.back() = pop;
      //    PUSH_TX( db.get(), trx, ~0 );
      // }

      trx.operations.clear();
      auto nathan = create_account("nathan");
      op.issuer = account_id_type();
      op.new_issuer = nathan.id;
      trx.operations.emplace_back(op);
      PUSH_TX( db.get(), trx, ~0 );
      BOOST_CHECK(bit_usd.issuer == nathan.id);

      op.issuer = nathan.id;
      op.new_issuer = account_id_type();
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );
      BOOST_CHECK(bit_usd.issuer == account_id_type());
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}


BOOST_AUTO_TEST_CASE( create_uia )
{
   try {
      asset_id_type test_asset_id = db->get_index<asset_object>().get_next_id();
      asset_create_operation creator;
      creator.issuer = account_id_type();
      // creator.fee = asset();
      creator.symbol = UIA_TEST_SYMBOL;
      creator.common_options.max_supply = 100000000;
      creator.precision = 2;
      creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
      creator.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
      creator.common_options.flags = charge_market_fee;
      // creator.common_options.core_exchange_rate = price({asset(2),asset(1,asset_id_type(1))});
      trx.operations.push_back(std::move(creator));
      PUSH_TX( db.get(), trx, ~0 );

      const asset_object& test_asset = test_asset_id(*db);
      BOOST_CHECK(test_asset.symbol == UIA_TEST_SYMBOL);
      // BOOST_CHECK(asset(1, test_asset_id) * test_asset.options.core_exchange_rate == asset(2));
      BOOST_CHECK((test_asset.options.flags & white_list) == 0);
      BOOST_CHECK(test_asset.options.max_supply == 100000000);
      BOOST_CHECK(!test_asset.bitasset_data_id.valid());
      BOOST_CHECK(test_asset.options.market_fee_percent == GRAPHENE_MAX_MARKET_FEE_PERCENT/100);
      // GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx, ~0 ), fc::exception);

      const asset_dynamic_data_object& test_asset_dynamic_data = test_asset.dynamic_asset_data_id(*db);
      BOOST_CHECK(test_asset_dynamic_data.current_supply == 0);
      BOOST_CHECK(test_asset_dynamic_data.accumulated_fees == 0);

      auto op = trx.operations.back().get<asset_create_operation>();
      op.symbol = "TESTFAIL";
      REQUIRE_THROW_WITH_VALUE(op, issuer, account_id_type(99999999));
      // REQUIRE_THROW_WITH_VALUE(op, common_options.max_supply, -1);
      // REQUIRE_THROW_WITH_VALUE(op, common_options.max_supply, 0);
      // REQUIRE_THROW_WITH_VALUE(op, symbol, "A");
      // REQUIRE_THROW_WITH_VALUE(op, symbol, "qqq");
      // REQUIRE_THROW_WITH_VALUE(op, symbol, "11");
      // REQUIRE_THROW_WITH_VALUE(op, symbol, ".AAA");
      // REQUIRE_THROW_WITH_VALUE(op, symbol, "AAA.");
      // REQUIRE_THROW_WITH_VALUE(op, symbol, "AB CD");
      // REQUIRE_THROW_WITH_VALUE(op, symbol, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      // REQUIRE_THROW_WITH_VALUE(op, common_options.core_exchange_rate, price({asset(-100), asset(1)}));
      // REQUIRE_THROW_WITH_VALUE(op, common_options.core_exchange_rate, price({asset(100),asset(-1)}));
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/*
BOOST_AUTO_TEST_CASE( update_uia )
{
   using namespace graphene;
   try {
      INVOKE(create_uia);
      const auto& test = get_asset(UIA_TEST_SYMBOL);
      const auto& nathan = create_account("nathan");

      asset_update_operation op;
      op.issuer = test.issuer;
      op.asset_to_update = test.id;
      op.new_options = test.options;

      trx.operations.push_back(op);

      //Cannot change issuer to antelopee as before
      BOOST_TEST_MESSAGE( "Make sure changing issuer to antelopee as before is forbidden" );
      // REQUIRE_THROW_WITH_VALUE(op, new_issuer, test.issuer);

      //Cannot convert to an MIA
      BOOST_TEST_MESSAGE( "Make sure we can't convert UIA to MIA" );
      REQUIRE_THROW_WITH_VALUE(op, new_options.issuer_permissions, ASSET_ISSUER_PERMISSION_MASK);
      REQUIRE_THROW_WITH_VALUE(op, new_options.core_exchange_rate, price(asset(5), asset(5)));

      BOOST_TEST_MESSAGE( "Test updating core_exchange_rate" );
      op.new_options.core_exchange_rate = price(asset(3), test.amount(5));
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );
      REQUIRE_THROW_WITH_VALUE(op, new_options.core_exchange_rate, price());
      op.new_options.core_exchange_rate = test.options.core_exchange_rate;
      op.new_issuer = nathan.id;
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );

      BOOST_TEST_MESSAGE( "Test setting flags" );
      op.issuer = nathan.id;
      op.new_issuer.reset();
      op.new_options.flags = transfer_restricted | white_list;
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );

      BOOST_TEST_MESSAGE( "Disable white_list permission" );
      op.new_options.issuer_permissions = test.options.issuer_permissions & ~white_list;
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );

      BOOST_TEST_MESSAGE( "Can't toggle white_list" );
      REQUIRE_THROW_WITH_VALUE(op, new_options.flags, test.options.flags & ~white_list);

      BOOST_TEST_MESSAGE( "Can toggle transfer_restricted" );
      for( int i=0; i<2; i++ )
      {
         op.new_options.flags = test.options.flags ^ transfer_restricted;
         trx.operations.back() = op;
         PUSH_TX( db.get(), trx, ~0 );
      }

      BOOST_TEST_MESSAGE( "Make sure white_list can't be re-enabled" );
      op.new_options.issuer_permissions = test.options.issuer_permissions;
      op.new_options.flags = test.options.flags;
      BOOST_CHECK(!(test.options.issuer_permissions & white_list));
      REQUIRE_THROW_WITH_VALUE(op, new_options.issuer_permissions, UIA_ASSET_ISSUER_PERMISSION_MASK);

      BOOST_TEST_MESSAGE( "We can change issuer to account_id_type(), but can't do it again" );
      op.new_issuer = account_id_type();
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );
      op.issuer = account_id_type();
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx, ~0 ), fc::exception);
      op.new_issuer.reset();
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
*/

BOOST_AUTO_TEST_CASE( issue_uia )
{
   try {
      INVOKE(create_uia);
      INVOKE(create_account_test);

      const asset_object& test_asset = *db->get_index_type<asset_index>().indices().get<by_symbol>().find(UIA_TEST_SYMBOL);
      const account_object& nathan_account = *db->get_index_type<account_index>().indices().get<by_name>().find("nathan");

      asset_issue_operation op;
      op.issuer = test_asset.issuer;
      op.asset_to_issue =  test_asset.amount(5000000);
      op.issue_to_account = nathan_account.id;
      trx.operations.push_back(op);

      REQUIRE_THROW_WITH_VALUE(op, asset_to_issue, asset(200));
      REQUIRE_THROW_WITH_VALUE(op, issue_to_account, account_id_type(999999999));

      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );

      const asset_dynamic_data_object& test_dynamic_data = test_asset.dynamic_asset_data_id(*db);
      BOOST_CHECK_EQUAL(get_balance(nathan_account, test_asset), 5000000);
      BOOST_CHECK(test_dynamic_data.current_supply == 5000000);
      BOOST_CHECK(test_dynamic_data.accumulated_fees == 0);

      PUSH_TX( db.get(), trx, ~0 );

      BOOST_CHECK_EQUAL(get_balance(nathan_account, test_asset), 10000000);
      BOOST_CHECK(test_dynamic_data.current_supply == 10000000);
      BOOST_CHECK(test_dynamic_data.accumulated_fees == 0);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer_uia )
{
   try {
      INVOKE(issue_uia);

      const asset_object& uia = *db->get_index_type<asset_index>().indices().get<by_symbol>().find(UIA_TEST_SYMBOL);
      const account_object& nathan = *db->get_index_type<account_index>().indices().get<by_name>().find("nathan");
      const account_object& committee = account_id_type()(*db);

      BOOST_CHECK_EQUAL(get_balance(nathan, uia), 10000000);
      transfer_operation top;
      top.from = nathan.id;
      top.to = committee.id;
      top.amount = uia.amount(5000);
      trx.operations.push_back(top);
      BOOST_TEST_MESSAGE( "Transfering 5000 TEST from nathan to committee" );
      PUSH_TX( db.get(), trx, ~0 );
      BOOST_CHECK_EQUAL(get_balance(nathan, uia), 10000000 - 5000);
      BOOST_CHECK_EQUAL(get_balance(committee, uia), 5000);

      PUSH_TX( db.get(), trx, ~0 );
      BOOST_CHECK_EQUAL(get_balance(nathan, uia), 10000000 - 10000);
      BOOST_CHECK_EQUAL(get_balance(committee, uia), 10000);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/*
BOOST_AUTO_TEST_CASE( create_buy_uia_multiple_match_new )
{
 try {
   INVOKE( issue_uia );
   const asset_object&   core_asset     = get_asset( UIA_TEST_SYMBOL );
   const asset_object&   test_asset     = get_asset( GRAPHENE_SYMBOL );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );
   const account_object& seller_account = create_account( "seller" );

   transfer( committee_account(*db), buyer_account, test_asset.amount( 10000 ) );
   transfer( nathan_account, seller_account, core_asset.amount(10000) );

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );

   limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(100) )->id;
   limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(200) )->id;
   limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(300) )->id;

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

   //print_market( "", "" );
   auto unmatched = create_sell_order( seller_account, core_asset.amount(300), test_asset.amount(150) );
   //print_market( "", "" );
   BOOST_CHECK( !db->find( first_id ) );
   BOOST_CHECK( !db->find( second_id ) );
   BOOST_CHECK( db->find( third_id ) );
   if( unmatched ) wdump((*unmatched));
   BOOST_CHECK( !unmatched );

   BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 200 );
   BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 297 );
   BOOST_CHECK_EQUAL( core_asset.dynamic_asset_data_id(*db).accumulated_fees.value , 3 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}

BOOST_AUTO_TEST_CASE( create_buy_exact_match_uia )
{
   try {
      INVOKE( issue_uia );
      const asset_object&   test_asset     = get_asset( UIA_TEST_SYMBOL );
      const asset_object&   core_asset     = get_asset( GRAPHENE_SYMBOL );
      const account_object& nathan_account = get_account( "nathan" );
      const account_object& buyer_account  = create_account( "buyer" );
      const account_object& seller_account = create_account( "seller" );

      transfer( committee_account(*db), seller_account, asset( 10000 ) );
      transfer( nathan_account, buyer_account, test_asset.amount(10000) );

      BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );

      limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(100) )->id;
      limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(200) )->id;
      limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(300) )->id;

      BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

      //print_market( "", "" );
      auto unmatched = create_sell_order( seller_account, core_asset.amount(100), test_asset.amount(100) );
      //print_market( "", "" );
      BOOST_CHECK( !db->find( first_id ) );
      BOOST_CHECK( db->find( second_id ) );
      BOOST_CHECK( db->find( third_id ) );
      if( unmatched ) wdump((*unmatched));
      BOOST_CHECK( !unmatched );

      BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 99 );
      BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 100 );
      BOOST_CHECK_EQUAL( test_asset.dynamic_asset_data_id(*db).accumulated_fees.value , 1 );
   }
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}


BOOST_AUTO_TEST_CASE( create_buy_uia_multiple_match_new_reverse )
{
   try {
      INVOKE( issue_uia );
      const asset_object&   test_asset     = get_asset( UIA_TEST_SYMBOL );
      const asset_object&   core_asset     = get_asset( GRAPHENE_SYMBOL );
      const account_object& nathan_account = get_account( "nathan" );
      const account_object& buyer_account  = create_account( "buyer" );
      const account_object& seller_account = create_account( "seller" );

      transfer( committee_account(*db), seller_account, asset( 10000 ) );
      transfer( nathan_account, buyer_account, test_asset.amount(10000));

      BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );

      limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(100) )->id;
      limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(200) )->id;
      limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(300) )->id;

      BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

      //print_market( "", "" );
      auto unmatched = create_sell_order( seller_account, core_asset.amount(300), test_asset.amount(150) );
      //print_market( "", "" );
      BOOST_CHECK( !db->find( first_id ) );
      BOOST_CHECK( !db->find( second_id ) );
      BOOST_CHECK( db->find( third_id ) );
      if( unmatched ) wdump((*unmatched));
      BOOST_CHECK( !unmatched );

      BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 198 );
      BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 300 );
      BOOST_CHECK_EQUAL( test_asset.dynamic_asset_data_id(*db).accumulated_fees.value , 2 );
   }
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}


BOOST_AUTO_TEST_CASE( create_buy_uia_multiple_match_new_reverse_fract )
{
   try {
      INVOKE( issue_uia );
      const asset_object&   test_asset     = get_asset( UIA_TEST_SYMBOL );
      const asset_object&   core_asset     = get_asset( GRAPHENE_SYMBOL );
      const account_object& nathan_account = get_account( "nathan" );
      const account_object& buyer_account  = create_account( "buyer" );
      const account_object& seller_account = create_account( "seller" );

      transfer( committee_account(*db), seller_account, asset( 30 ) );
      transfer( nathan_account, buyer_account, test_asset.amount(10000) );

      BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );
      BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 0 );
      BOOST_CHECK_EQUAL( get_balance( seller_account, core_asset ), 30 );

      limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(10) )->id;
      limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(20) )->id;
      limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(30) )->id;

      BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

      //print_market( "", "" );
      auto unmatched = create_sell_order( seller_account, core_asset.amount(30), test_asset.amount(150) );
      //print_market( "", "" );
      BOOST_CHECK( !db->find( first_id ) );
      BOOST_CHECK( !db->find( second_id ) );
      BOOST_CHECK( db->find( third_id ) );
      if( unmatched ) wdump((*unmatched));
      BOOST_CHECK( !unmatched );

      BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 198 );
      BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 30 );
      BOOST_CHECK_EQUAL( get_balance( seller_account, core_asset ), 0 );
      BOOST_CHECK_EQUAL( test_asset.dynamic_asset_data_id(*db).accumulated_fees.value , 2 );
   }
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}
 */
BOOST_AUTO_TEST_CASE( cancel_limit_order_test )
{
   try {
      INVOKE( issue_uia );
      const asset_object&   test_asset     = get_asset( UIA_TEST_SYMBOL );
      const account_object& buyer_account  = create_account( "buyer" );

      transfer( committee_account(*db), buyer_account, asset( 10000 ) );

      BOOST_CHECK_EQUAL( get_balance(buyer_account, asset_id_type()(*db)), 10000 );
      auto sell_order = create_sell_order( buyer_account, asset(1000), test_asset.amount(100+450*1) );
      FC_ASSERT( sell_order );
      auto refunded = cancel_limit_order( *sell_order );
      BOOST_CHECK( refunded == asset(1000) );
      BOOST_CHECK_EQUAL( get_balance(buyer_account, asset_id_type()(*db)), 10000 );
   }
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

/*
BOOST_AUTO_TEST_CASE( witness_feeds )
{
   using namespace graphene::chain;
   try {
      INVOKE( create_mia );
      {
         auto& current = get_asset( "USDBIT" );
         asset_update_operation uop;
         uop.issuer =  current.issuer;
         uop.asset_to_update = current.id;
         uop.new_options = current.options;
         uop.new_issuer = account_id_type();
         trx.operations.push_back(uop);
         PUSH_TX( db.get(), trx, ~0 );
         trx.clear();
      }
      generate_block();
      const asset_object& bit_usd = get_asset("USDBIT");
      auto& global_props = db->get_global_properties();
      vector<account_id_type> active_witnesses;
      for( const witness_id_type& wit_id : global_props.active_witnesses )
         active_witnesses.push_back( wit_id(*db).witness_account );
      BOOST_REQUIRE_EQUAL(active_witnesses.size(), 11);

      asset_publish_feed_operation op;
      op.publisher = active_witnesses[0];
      op.asset_id = bit_usd.get_id();
      op.feed.settlement_price = ~price(asset(GRAPHENE_BLOCKCHAIN_PRECISION),bit_usd.amount(30));
      // Accept defaults for required collateral
      trx.operations.emplace_back(op);
      PUSH_TX( db.get(), trx, ~0 );

      const asset_bitasset_data_object& bitasset = bit_usd.bitasset_data(*db);
      BOOST_CHECK(bitasset.current_feed.settlement_price.to_real() == 30.0 / GRAPHENE_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bitasset.current_feed.maintenance_collateral_ratio == GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.publisher = active_witnesses[1];
      op.feed.settlement_price = ~price(asset(GRAPHENE_BLOCKCHAIN_PRECISION),bit_usd.amount(25));
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );

      BOOST_CHECK_EQUAL(bitasset.current_feed.settlement_price.to_real(), 30.0 / GRAPHENE_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bitasset.current_feed.maintenance_collateral_ratio == GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.publisher = active_witnesses[2];
      op.feed.settlement_price = ~price(asset(GRAPHENE_BLOCKCHAIN_PRECISION),bit_usd.amount(40));
      // But this witness is an idiot.
      op.feed.maintenance_collateral_ratio = 1001;
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );

      BOOST_CHECK_EQUAL(bitasset.current_feed.settlement_price.to_real(), 30.0 / GRAPHENE_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bitasset.current_feed.maintenance_collateral_ratio == GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);
   } catch (const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
 */
/**
 *  Create an order such that when the trade executes at the
 *  requested price the resulting payout to one party is 0
 */
/*
BOOST_AUTO_TEST_CASE( trade_amount_equals_zero )
{
   try {
      INVOKE(issue_uia);

      set_expiration( db.get(), trx );

      const asset_object& test = get_asset( UIA_TEST_SYMBOL );
      const asset_id_type test_id = test.id;
      const asset_object& core = get_asset( GRAPHENE_SYMBOL );
      const asset_id_type core_id = core.id;
      const account_object& core_seller = create_account( "shorter1" );
      const account_object& core_buyer = get_account("nathan");

      transfer( committee_account(*db), core_seller, asset( 100000000 ) );

      BOOST_CHECK_EQUAL(get_balance(core_buyer, core), 0);
      BOOST_CHECK_EQUAL(get_balance(core_buyer, test), 10000000);
      BOOST_CHECK_EQUAL(get_balance(core_seller, test), 0);
      BOOST_CHECK_EQUAL(get_balance(core_seller, core), 100000000);

      create_sell_order(core_seller, core.amount(1), test.amount(2));
      create_sell_order(core_seller, core.amount(1), test.amount(2));
      create_sell_order(core_buyer, test.amount(3), core.amount(1));

      BOOST_CHECK_EQUAL(get_balance(core_buyer, core), 1);
      BOOST_CHECK_EQUAL(get_balance(core_buyer, test), 9999997);
      BOOST_CHECK_EQUAL(get_balance(core_seller, core), 99999998);
      BOOST_CHECK_EQUAL(get_balance(core_seller, test), 3);

      generate_block();
      fc::usleep(fc::milliseconds(6000));

       //TODO: This will fail because of something-for-nothing bug(#345)
       // Must be fixed with a hardfork
      auto result = get_market_order_history(core_id, test_id);
      BOOST_CHECK_EQUAL(result.size(), 2);
      BOOST_CHECK(result[0].op.pays == core.amount(1));
      BOOST_CHECK(result[0].op.receives == test.amount(2));
      BOOST_CHECK(result[1].op.pays == test.amount(2));
      BOOST_CHECK(result[1].op.receives == core.amount(1));
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
 */

/**
 *  Create an order that cannot be filled immediately and have the
 *  transaction fail.
 */
BOOST_AUTO_TEST_CASE( limit_order_fill_or_kill )
{
   try {
      INVOKE(issue_uia);
      const account_object& nathan = get_account("nathan");
      const asset_object& test = get_asset(UIA_TEST_SYMBOL);
      const asset_object& core = asset_id_type()(*db);

      limit_order_create_operation op;
      op.seller = nathan.id;
      op.amount_to_sell = test.amount(500);
      op.min_to_receive = core.amount(500);
      op.fill_or_kill = true;

      trx.operations.clear();
      trx.operations.push_back(op);
      GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, ~0 ), fc::exception);
      op.fill_or_kill = false;
      trx.operations.back() = op;
      PUSH_TX( db.get(), trx, ~0 );
   } FC_LOG_AND_RETHROW()
}

/// Shameless code coverage plugging. Otherwise, these calls never happen.
BOOST_AUTO_TEST_CASE( fill_order )
{
   try {
      fill_order_operation o;
      GRAPHENE_CHECK_THROW(o.validate(), fc::exception);
      //o.calculate_fee(db->current_fee_schedule());
   } FC_LOG_AND_RETHROW()
}

/*
BOOST_AUTO_TEST_CASE( witness_pay_test )
{
   try {
      const share_type prec = asset::scaled_precision( asset_id_type()(*db).precision );

      // there is an immediate maintenance interval in the first block
      //  which will initialize last_budget_time
      generate_block();

      // Make an account and upgrade it to prime, so that witnesses get some pay
      create_account("nathan", init_account_pub_key);
      transfer(account_id_type()(*db), get_account("nathan"), asset(20000*prec));
      transfer(account_id_type()(*db), get_account("init3"), asset(20*prec));
      generate_block();

      auto last_witness_vbo_balance = [&]() -> share_type
      {
         const witness_object& wit = db->fetch_block_by_number(db->head_block_num())->witness(*db);
         if( !wit.pay_vb.valid() )
            return 0;
         return (*wit.pay_vb)(*db).balance.amount;
      };

      const auto block_interval = db->get_global_properties().parameters.block_interval;
      const asset_object* core = &asset_id_type()(*db);
      const account_object* nathan = &get_account("nathan");
      enable_fees();
      BOOST_CHECK_GT(db->current_fee_schedule().get<account_upgrade_operation>().membership_lifetime_fee, 0);
      // Based on the size of the reserve fund later in the test, the witness budget will be set to this value
      const uint64_t ref_budget =
         ((uint64_t( db->current_fee_schedule().get<account_upgrade_operation>().membership_lifetime_fee )
            * GRAPHENE_CORE_ASSET_CYCLE_RATE * 30
            * block_interval
         ) + ((uint64_t(1) << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS)-1)
         ) >> GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS
         ;
      // change this if ref_budget changes
      BOOST_CHECK_EQUAL( ref_budget, 594 );
      const uint64_t witness_ppb = ref_budget * 10 / 23 + 1;
      // change this if ref_budget changes
      BOOST_CHECK_EQUAL( witness_ppb, 259 );
      // following two inequalities need to hold for maximal code coverage
      BOOST_CHECK_LT( witness_ppb * 2, ref_budget );
      BOOST_CHECK_GT( witness_ppb * 3, ref_budget );

      db->modify( db->get_global_properties(), [&]( global_property_object& _gpo )
      {
         _gpo.parameters.witness_pay_per_block = witness_ppb;
      } );

      BOOST_CHECK_EQUAL(core->dynamic_asset_data_id(*db).accumulated_fees.value, 0);
      BOOST_TEST_MESSAGE( "Upgrading account" );
      account_upgrade_operation uop;
      uop.account_to_upgrade = nathan->get_id();
      uop.upgrade_to_lifetime_member = true;
      set_expiration( db.get(), trx );
      trx.operations.push_back(uop);
      // for( auto& op : trx.operations ) db->current_fee_schedule().set_fee(op);
      trx.validate();
      sign( trx, init_account_priv_key );
      PUSH_TX( db.get(), trx );
      auto pay_fee_time = db->head_block_time().sec_since_epoch();
      trx.clear();
      // BOOST_CHECK( get_balance(*nathan, *core) == 20000*prec - account_upgrade_operation::fee_parameters_type().membership_lifetime_fee );;

      generate_block();
      nathan = &get_account("nathan");
      core = &asset_id_type()(*db);
      BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );

      auto schedule_maint = [&]()
      {
         // now we do maintenance
         db->modify( db->get_dynamic_global_properties(), [&]( dynamic_global_property_object& _dpo )
         {
            _dpo.next_maintenance_time = db->head_block_time() + 1;
         } );
      };
      BOOST_TEST_MESSAGE( "Generating some blocks" );

      // generate some blocks
      while( db->head_block_time().sec_since_epoch() - pay_fee_time < 24 * block_interval )
      {
         generate_block();
         BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );
      }
      BOOST_CHECK_EQUAL( db->head_block_time().sec_since_epoch() - pay_fee_time, 24 * block_interval );

      schedule_maint();
      // The 80% lifetime referral fee went to the committee account, which burned it. Check that it's here.
      BOOST_CHECK( core->reserved(*db).value == 8000*prec );
      generate_block();
      BOOST_CHECK_EQUAL( core->reserved(*db).value, 999999406 );
      BOOST_CHECK_EQUAL( db->get_dynamic_global_properties().witness_budget.value, ref_budget );
      // first witness paid from old budget (so no pay)
      BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );
      // second witness finally gets paid!
      generate_block();
      BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, witness_ppb );
      BOOST_CHECK_EQUAL( db->get_dynamic_global_properties().witness_budget.value, ref_budget - witness_ppb );

      generate_block();
      BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, witness_ppb );
      BOOST_CHECK_EQUAL( db->get_dynamic_global_properties().witness_budget.value, ref_budget - 2 * witness_ppb );

      generate_block();
      BOOST_CHECK_LT( last_witness_vbo_balance().value, witness_ppb );
      BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, ref_budget - 2 * witness_ppb );
      BOOST_CHECK_EQUAL( db->get_dynamic_global_properties().witness_budget.value, 0 );

      generate_block();
      BOOST_CHECK_EQUAL( last_witness_vbo_balance().value, 0 );
      BOOST_CHECK_EQUAL( db->get_dynamic_global_properties().witness_budget.value, 0 );
      BOOST_CHECK_EQUAL(core->reserved(*db).value, 999999406 );

   } FC_LOG_AND_RETHROW()
}
 */
/**
 *  Reserve asset test should make sure that all assets except bitassets
 *  can be burned, and all supplies add up.
 */
/*
BOOST_AUTO_TEST_CASE( reserve_asset_test )
{
   try {
      ACTORS((mustang)(dingss)(antelope)(judge));
      const auto& basset = create_bitasset("USDBIT", judge_id);
      const auto& uasset = create_user_issued_asset(UIA_TEST_SYMBOL);
      const auto& passet = create_prediction_market("PMARK", judge_id);
      const auto& casset = asset_id_type()(*db);

      auto reserve_asset = [&]( account_id_type payer, asset amount_to_reserve )
      {
         asset_reserve_operation op;
         op.payer = payer;
         op.amount_to_reserve = amount_to_reserve;
         transaction tx;
         tx.operations.push_back( op );
         set_expiration( db.get(), tx );
         db->push_transaction( tx, database::skip_authority_check | database::skip_tapos_check | database::skip_transaction_signatures );
      } ;

      auto _issue_uia = [&]( const account_object& recipient, asset amount )
      {
         asset_issue_operation op;
         op.issuer = amount.asset_id(*db).issuer;
         op.asset_to_issue = amount;
         op.issue_to_account = recipient.id;
         transaction tx;
         tx.operations.push_back( op );
         set_expiration( db.get(), tx );
         db->push_transaction( tx, database::skip_authority_check | database::skip_tapos_check | database::skip_transaction_signatures );
      } ;

      int64_t init_balance = 10000;
      int64_t reserve_amount = 3000;
      share_type initial_reserve;

      BOOST_TEST_MESSAGE( "Test reserve operation on core asset" );
      transfer( committee_account, mustang_id, casset.amount( init_balance ) );

      initial_reserve = casset.reserved( *db );
      reserve_asset( mustang_id, casset.amount( reserve_amount  ) );
      BOOST_CHECK_EQUAL( get_balance( mustang, casset ), init_balance - reserve_amount );
      BOOST_CHECK_EQUAL( (casset.reserved( *db ) - initial_reserve).value, reserve_amount );
      verify_asset_supplies(db.get());

      BOOST_TEST_MESSAGE( "Test reserve operation on market issued asset" );
      transfer( committee_account, mustang_id, casset.amount( init_balance*100 ) );
      update_feed_producers( basset, {antelope.id} );
      price_feed current_feed;
      current_feed.settlement_price = basset.amount( 2 ) / casset.amount(100);
      current_feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different default
      publish_feed( basset, antelope, current_feed );
      borrow( mustang_id, basset.amount( init_balance ), casset.amount( 100*init_balance ) );
      BOOST_CHECK_EQUAL( get_balance( mustang, basset ), init_balance );

      GRAPHENE_REQUIRE_THROW( reserve_asset( mustang_id, basset.amount( reserve_amount ) ), asset_reserve_invalid_on_mia );

      BOOST_TEST_MESSAGE( "Test reserve operation on prediction market asset" );
      transfer( committee_account, mustang_id, casset.amount( init_balance ) );
      borrow( mustang_id, passet.amount( init_balance ), casset.amount( init_balance ) );
      GRAPHENE_REQUIRE_THROW( reserve_asset( mustang_id, passet.amount( reserve_amount ) ), asset_reserve_invalid_on_mia );

      BOOST_TEST_MESSAGE( "Test reserve operation on user issued asset" );
      _issue_uia( mustang, uasset.amount( init_balance ) );
      BOOST_CHECK_EQUAL( get_balance( mustang, uasset ), init_balance );
      verify_asset_supplies(db.get());

      BOOST_TEST_MESSAGE( "Reserving asset" );
      initial_reserve = uasset.reserved( *db );
      reserve_asset( mustang_id, uasset.amount( reserve_amount  ) );
      BOOST_CHECK_EQUAL( get_balance( mustang, uasset ), init_balance - reserve_amount );
      BOOST_CHECK_EQUAL( (uasset.reserved( *db ) - initial_reserve).value, reserve_amount );
      verify_asset_supplies(db.get());
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}
 */

/**
 * This test demonstrates how using the call_order_update_operation to
 * trigger a margin call is legal if there is a matching order.
 */
/*
BOOST_AUTO_TEST_CASE( cover_with_collateral_test )
{
   try {
      ACTORS((mustang)(dingss)(antelope));
      const auto& bitusd = create_bitasset("USDBIT", antelope_id);
      const auto& core   = asset_id_type()(*db);

      BOOST_TEST_MESSAGE( "Setting price feed to $0.02 / 100" );
      transfer(committee_account, mustang_id, asset(10000000));
      update_feed_producers( bitusd, {antelope.id} );

      price_feed current_feed;
      current_feed.settlement_price = bitusd.amount( 2 ) / core.amount(100);
      publish_feed( bitusd, antelope, current_feed );

      BOOST_REQUIRE( bitusd.bitasset_data(*db).current_feed.settlement_price == current_feed.settlement_price );

      BOOST_TEST_MESSAGE( "mustang borrows some BitUSD at 2x collateral and gives it to dingss" );
      const call_order_object* call_order = borrow( mustang, bitusd.amount(100), asset(10000) );
      BOOST_REQUIRE( call_order != nullptr );

      // wdump( (*call_order) );

      transfer( mustang_id, dingss_id, bitusd.amount(100) );

      auto update_call_order = [&]( account_id_type acct, asset delta_collateral, asset delta_debt )
      {
         call_order_update_operation op;
         op.funding_account = acct;
         op.delta_collateral = delta_collateral;
         op.delta_debt = delta_debt;
         transaction tx;
         tx.operations.push_back( op );
         set_expiration( db.get(), tx );
         db->push_transaction( tx, database::skip_authority_check | database::skip_tapos_check | database::skip_transaction_signatures );
      } ;

      // margin call requirement:  1.75x
      BOOST_TEST_MESSAGE( "mustang decreases her collateral to maint level plus one satoshi" );
      asset delta_collateral = asset(int64_t( current_feed.maintenance_collateral_ratio ) * 5000 / GRAPHENE_COLLATERAL_RATIO_DENOM - 10000 + 1 );
      update_call_order( mustang_id, delta_collateral, bitusd.amount(0) );
      // wdump( (*call_order) );

      BOOST_TEST_MESSAGE( "mustang cannot decrease her collateral by one satoshi, there is no buyer" );
      GRAPHENE_REQUIRE_THROW( update_call_order( mustang_id, asset(-1), bitusd.amount(0) ), call_order_update_unfilled_margin_call );
      // wdump( (*call_order) );

      BOOST_TEST_MESSAGE( "dingss offers to sell most of the BitUSD at the feed" );
      const limit_order_object* order = create_sell_order( dingss_id, bitusd.amount(99), asset(4950) );
      BOOST_REQUIRE( order != nullptr );
      limit_order_id_type order1_id = order->id;
      BOOST_CHECK_EQUAL( order->for_sale.value, 99 );
      // wdump( (*call_order) );

      BOOST_TEST_MESSAGE( "mustang still cannot decrease her collateral to maint level" );
      GRAPHENE_REQUIRE_THROW( update_call_order( mustang_id, asset(-1), bitusd.amount(0) ), call_order_update_unfilled_margin_call );
      // wdump( (*call_order) );

      BOOST_TEST_MESSAGE( "dingss offers to sell the last of his BitUSD in another order" );
      order = create_sell_order( dingss_id, bitusd.amount(1), asset(50) );
      BOOST_REQUIRE( order != nullptr );
      limit_order_id_type order2_id = order->id;
      BOOST_CHECK_EQUAL( order->for_sale.value, 1 );
      // wdump( (*call_order) );

      BOOST_TEST_MESSAGE( "mustang decreases her collateral to maint level and dingss's orders fill" );
      update_call_order( mustang_id, asset(-1), bitusd.amount(0) );

      BOOST_CHECK( db->find( order1_id ) == nullptr );
      BOOST_CHECK( db->find( order2_id ) == nullptr );
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}
 */

BOOST_AUTO_TEST_CASE( vesting_balance_create_test )
{
   try {
      INVOKE( create_uia );

      const asset_object& core = asset_id_type()(*db);
      const asset_object& test_asset = get_asset(UIA_TEST_SYMBOL);

      vesting_balance_create_operation op;
      // op.fee = core.amount( 0 );
      op.creator = account_id_type();
      op.owner = account_id_type();
      op.amount = test_asset.amount( 100 );
      //op.vesting_seconds = 60*60*24;
      op.policy = cdd_vesting_policy_initializer{ 60*60*24 };

      // Fee must be non-negative
      // REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(1) );
      // REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(0) );
      // REQUIRE_OP_VALIDATION_FAILURE( op, fee, core.amount(-1) );

      // Amount must be positive
      REQUIRE_OP_VALIDATION_SUCCESS( op, amount, core.amount(1) );
      REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount(0) );
      REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount(-1) );

      // Setup world state we will need to test actual evaluation
      const account_object& mustang_account = create_account("mustang");
      const account_object& dingss_account = create_account("dingss");

      transfer(committee_account(*db), mustang_account, core.amount(100000));

      op.creator = mustang_account.get_id();
      op.owner = mustang_account.get_id();

      account_id_type nobody = account_id_type(1234);

      trx.operations.push_back(op);
      // Invalid account_id's
      REQUIRE_THROW_WITH_VALUE( op, creator, nobody );
      REQUIRE_THROW_WITH_VALUE( op,   owner, nobody );

      // Insufficient funds
      REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(999999999) );
      // mustang can fund a bond to herself or to dingss
      op.amount = core.amount( 1000 );
      REQUIRE_OP_EVALUATION_SUCCESS( op, owner, mustang_account.get_id() );
      REQUIRE_OP_EVALUATION_SUCCESS( op, owner,   dingss_account.get_id() );
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( vesting_balance_withdraw_test )
{
   try {
      INVOKE( create_uia );
      // required for head block time
      generate_block();

      const asset_object& core = asset_id_type()(*db);
      const asset_object& test_asset = get_asset( UIA_TEST_SYMBOL );

      vesting_balance_withdraw_operation op;
      // op.fee = core.amount( 0 );
      op.vesting_balance = vesting_balance_id_type();
      op.owner = account_id_type();
      op.amount = test_asset.amount( 100 );

      // Fee must be non-negative
      // REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(  1 )  );
      // REQUIRE_OP_VALIDATION_SUCCESS( op, fee, core.amount(  0 )  );
      // REQUIRE_OP_VALIDATION_FAILURE( op, fee, core.amount( -1 ) );

      // Amount must be positive
      REQUIRE_OP_VALIDATION_SUCCESS( op, amount, core.amount(  1 ) );
      REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount(  0 ) );
      REQUIRE_OP_VALIDATION_FAILURE( op, amount, core.amount( -1 ) );

      // Setup world state we will need to test actual evaluation
      const account_object& mustang_account = create_account( "mustang" );
      const account_object& dingss_account = create_account( "dingss" );

      transfer( committee_account(*db), mustang_account, core.amount( 1000000 ) );

      auto spin_vbo_clock = [&]( const vesting_balance_object& vbo, uint32_t dt_secs )
      {
         // HACK:  This just modifies the DB creation record to be further
         //   in the past
         db->modify( vbo, [&]( vesting_balance_object& _vbo )
         {
            _vbo.policy.get<cdd_vesting_policy>().coin_seconds_earned_last_update -= dt_secs;
         } );
      };

      auto create_vbo = [&](
         account_id_type creator, account_id_type owner,
         asset amount, uint32_t vesting_seconds, uint32_t elapsed_seconds
         ) -> const vesting_balance_object&
      {
         transaction tx;

         vesting_balance_create_operation create_op;
         // create_op.fee = core.amount( 0 );
         create_op.creator = creator;
         create_op.owner = owner;
         create_op.amount = amount;
         create_op.policy = cdd_vesting_policy_initializer(vesting_seconds);
         tx.operations.push_back( create_op );
         set_expiration( db.get(), tx );

         processed_transaction ptx = PUSH_TX( db.get(),  tx, ~0  );
         const vesting_balance_object& vbo = vesting_balance_id_type(
            ptx.operation_results[0].get<object_id_result>().result)(*db);

         if( elapsed_seconds > 0 )
            spin_vbo_clock( vbo, elapsed_seconds );
         return vbo;
      };

      auto top_up = [&]()
      {
         trx.clear();
         transfer( committee_account(*db),
            mustang_account,
            core.amount( 1000000 - db->get_balance( mustang_account, core ).amount )
            );
         FC_ASSERT( db->get_balance( mustang_account, core ).amount == 1000000 );
         trx.clear();
         trx.operations.push_back( op );
      };

      trx.clear();
      trx.operations.push_back( op );

      {
         // Try withdrawing a single satoshi
         const vesting_balance_object& vbo = create_vbo(
            mustang_account.id, mustang_account.id, core.amount( 10000 ), 1000, 0);

         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  990000 );

         op.vesting_balance = vbo.id;
         op.owner = mustang_account.id;

         REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(1) );

         // spin the clock and make sure we can withdraw 1/1000 in 1 second
         spin_vbo_clock( vbo, 1 );
         // mustang shouldn't be able to withdraw 11, it's too much
         REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(11) );
         op.amount = core.amount( 1 );
         // dingss shouldn't be able to withdraw anything
         REQUIRE_THROW_WITH_VALUE( op, owner, dingss_account.id );
         // Shouldn't be able to get out different asset than was put in
         REQUIRE_THROW_WITH_VALUE( op, amount, test_asset.amount(1) );
         // Withdraw the max, we are OK...
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(10) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  990010 );
         top_up();
      }

      // Make sure we can withdraw the correct amount after 999 seconds
      {
         const vesting_balance_object& vbo = create_vbo(
            mustang_account.id, mustang_account.id, core.amount( 10000 ), 1000, 999);

         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  990000 );

         op.vesting_balance = vbo.id;
         op.owner = mustang_account.id;
         // Withdraw one satoshi too much, no dice
         REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(9991) );
         // Withdraw just the right amount, success!
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(9990) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  999990 );
         top_up();
      }

      // Make sure we can withdraw the whole thing after 1000 seconds
      {
         const vesting_balance_object& vbo = create_vbo(
            mustang_account.id, mustang_account.id, core.amount( 10000 ), 1000, 1000);

         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  990000 );

         op.vesting_balance = vbo.id;
         op.owner = mustang_account.id;
         // Withdraw one satoshi too much, no dice
         REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(10001) );
         // Withdraw just the right amount, success!
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(10000) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount == 1000000 );
      }

      // Make sure that we can't withdraw a single extra satoshi no matter how old it is
      {
         const vesting_balance_object& vbo = create_vbo(
            mustang_account.id, mustang_account.id, core.amount( 10000 ), 1000, 123456);

         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  990000 );

         op.vesting_balance = vbo.id;
         op.owner = mustang_account.id;
         // Withdraw one satoshi too much, no dice
         REQUIRE_THROW_WITH_VALUE( op, amount, core.amount(10001) );
         // Withdraw just the right amount, success!
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(10000) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount == 1000000 );
      }

      // Try withdrawing in three max installments:
      //  5000 after  500      seconds
      //  2000 after  400 more seconds
      //  3000 after 1000 more seconds
      {
         const vesting_balance_object& vbo = create_vbo(
            mustang_account.id, mustang_account.id, core.amount( 10000 ), 1000, 0);

         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  990000 );

         op.vesting_balance = vbo.id;
         op.owner = mustang_account.id;
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(   1) );
         spin_vbo_clock( vbo, 499 );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(5000) );
         spin_vbo_clock( vbo,   1 );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(5001) );
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(5000) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  995000 );

         spin_vbo_clock( vbo, 399 );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(2000) );
         spin_vbo_clock( vbo,   1 );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(2001) );
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(2000) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  997000 );

         spin_vbo_clock( vbo, 999 );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(3000) );
         spin_vbo_clock( vbo, 1   );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(3001) );
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(3000) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount == 1000000 );
      }

      //
      // Increase by 10,000 csd / sec initially.
      // After 500 seconds, we have 5,000,000 csd.
      // Withdraw 2,000, we are now at 8,000 csd / sec.
      // At 8,000 csd / sec, it will take us 625 seconds to mature.
      //
      {
         const vesting_balance_object& vbo = create_vbo(
            mustang_account.id, mustang_account.id, core.amount( 10000 ), 1000, 0);

         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  990000 );

         op.vesting_balance = vbo.id;
         op.owner = mustang_account.id;
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(   1) );
         spin_vbo_clock( vbo, 500 );
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(2000) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount ==  992000 );

         spin_vbo_clock( vbo, 624 );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(8000) );
         spin_vbo_clock( vbo,   1 );
         REQUIRE_THROW_WITH_VALUE     ( op, amount, core.amount(8001) );
         REQUIRE_OP_EVALUATION_SUCCESS( op, amount, core.amount(8000) );
         FC_ASSERT( db->get_balance( mustang_account,       core ).amount == 1000000 );
      }
      // TODO:  Test with non-core asset and dingss account
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
