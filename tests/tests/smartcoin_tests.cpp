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

#include <graphene/app/database_api.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/exceptions.hpp>

#include <iostream>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;


BOOST_FIXTURE_TEST_SUITE(smartcoin_tests, database_fixture)


BOOST_AUTO_TEST_CASE(bsip36)
{
   try
   {
      /* Issue affects only smartcoins(market pegged assets feeded by active witnesses or committee members).
       * Test case reproduces, advance to hardfork and check if solved after it.
       */

      /* References:
       * BSIP 36: https://github.com/bitshares/bsips/blob/master/bsip-0036.md
       * and the former: CORE Issue 518: https://github.com/bitshares/bitshares-core/issues/518
       */

      // Create 12 accounts to be witnesses under our control
      ACTORS( (witness0)(witness1)(witness2)(witness3)(witness4)(witness5)
                   (witness6)(witness7)(witness8)(witness9)(witness10)(witness11) );

      // Upgrade all accounts to LTM
      upgrade_to_lifetime_member(witness0_id);
      upgrade_to_lifetime_member(witness1_id);
      upgrade_to_lifetime_member(witness2_id);
      upgrade_to_lifetime_member(witness3_id);
      upgrade_to_lifetime_member(witness4_id);
      upgrade_to_lifetime_member(witness5_id);
      upgrade_to_lifetime_member(witness6_id);
      upgrade_to_lifetime_member(witness7_id);
      upgrade_to_lifetime_member(witness8_id);
      upgrade_to_lifetime_member(witness9_id);
      upgrade_to_lifetime_member(witness10_id);
      upgrade_to_lifetime_member(witness11_id);

      // Create all the witnesses
      const witness_id_type witness0_witness_id = create_witness(witness0_id, witness0_private_key).id;
      const witness_id_type witness1_witness_id = create_witness(witness1_id, witness1_private_key).id;
      const witness_id_type witness2_witness_id = create_witness(witness2_id, witness2_private_key).id;
      const witness_id_type witness3_witness_id = create_witness(witness3_id, witness3_private_key).id;
      const witness_id_type witness4_witness_id = create_witness(witness4_id, witness4_private_key).id;
      const witness_id_type witness5_witness_id = create_witness(witness5_id, witness5_private_key).id;
      const witness_id_type witness6_witness_id = create_witness(witness6_id, witness6_private_key).id;
      const witness_id_type witness7_witness_id = create_witness(witness7_id, witness7_private_key).id;
      const witness_id_type witness8_witness_id = create_witness(witness8_id, witness8_private_key).id;
      const witness_id_type witness9_witness_id = create_witness(witness9_id, witness9_private_key).id;
      const witness_id_type witness10_witness_id = create_witness(witness10_id, witness10_private_key).id;
      const witness_id_type witness11_witness_id = create_witness(witness11_id, witness11_private_key).id;

      // Create a vector with private key of all witnesses, will be used to activate 11 witnesses at a time
      const vector <fc::ecc::private_key> private_keys = {
            witness0_private_key,
            witness1_private_key,
            witness2_private_key,
            witness3_private_key,
            witness4_private_key,
            witness5_private_key,
            witness6_private_key,
            witness7_private_key,
            witness8_private_key,
            witness9_private_key,
            witness10_private_key
      };

      // create a map with account id and witness id of the first 11 witnesses
      const flat_map <account_id_type, witness_id_type> witness_map = {
         {witness0_id, witness0_witness_id},
         {witness1_id, witness1_witness_id},
         {witness2_id, witness2_witness_id},
         {witness3_id, witness3_witness_id},
         {witness4_id, witness4_witness_id},
         {witness5_id, witness5_witness_id},
         {witness6_id, witness6_witness_id},
         {witness7_id, witness7_witness_id},
         {witness8_id, witness8_witness_id},
         {witness9_id, witness9_witness_id},
         {witness10_id, witness10_witness_id}
      };

      // Create the asset
      const asset_id_type bit_usd_id = create_bitasset("USDBIT").id;

      // Update the asset to be fed by system witnesses
      asset_update_operation op;
      const asset_object &asset_obj = bit_usd_id(*db);
      op.asset_to_update = bit_usd_id;
      op.issuer = asset_obj.issuer;
      op.new_options = asset_obj.options;
      op.new_options.flags &= witness_fed_asset;
      op.new_options.issuer_permissions &= witness_fed_asset;
      trx.operations.push_back(op);
      PUSH_TX(db.get(), trx, ~0);
      generate_block();
      trx.clear();

      // Check current default witnesses, default chain is configured with 10 witnesses
      auto witnesses = db->get_global_properties().active_witnesses;

      // We need to activate 11 witnesses by voting for each of them.
      // Each witness is voted with incremental stake so last witness created will be the ones with more votes


      int c = 0;
      for (auto l : witness_map) {
         // voting stake have step of 100
         // so vote_for_committee_and_witnesses() with stake=10 does not affect the expected result
         int stake = 100 * (c + 1);
         transfer(committee_account, l.first, asset(stake));
         {
            account_update_operation op;
            op.account = l.first;
            op.new_options = l.first(*db).options;
            op.new_options->votes.insert(l.second(*db).vote_id);

            trx.operations.push_back(op);
            sign(trx, private_keys.at(c));
            PUSH_TX(db.get(), trx);
            trx.clear();
         }
         ++c;
      }

      // Trigger the new witnesses
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      // Check my witnesses are now in control of the system
      witnesses = db->get_global_properties().active_witnesses;
      BOOST_CHECK_EQUAL(witnesses.size(), 11u);

      // Adding 2 feeds with witnesses 0 and 1, checking if they get inserted
      const asset_object &core = asset_id_type()(*db);
      price_feed feed;
      feed.settlement_price = bit_usd_id(*db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(*db), witness0_id(*db), feed);

      asset_bitasset_data_object bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      auto itr = bitasset_data.feeds.begin();

      feed.settlement_price = bit_usd_id(*db).amount(2) / core.amount(5);
      publish_feed(bit_usd_id(*db), witness1_id(*db), feed);

      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);

      // Activate witness11 with voting stake, will kick the witness with less votes(witness0) out of the active list
      transfer(committee_account, witness11_id, asset(1200));
      set_expiration(db.get(), trx);
      {
         account_update_operation op;
         op.account = witness11_id;
         op.new_options = witness11_id(*db).options;
         op.new_options->votes.insert(witness11_witness_id(*db).vote_id);
         trx.operations.push_back(op);
         sign(trx, witness11_private_key);
         PUSH_TX(db.get(), trx);
         trx.clear();
      }

      // Trigger new witness
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      // Check active witness list now
      witnesses = db->get_global_properties().active_witnesses;

      // witness0 has been removed but it was a feeder before
      // Feed persist in the blockchain, this reproduces the issue
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);

      // Feed persist after expiration
      const auto feed_lifetime = bit_usd_id(*db).bitasset_data(*db).options.feed_lifetime_sec;
      generate_blocks(db->head_block_time() + feed_lifetime + 1);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);

      // Other witnesses add more feeds
      feed.settlement_price = bit_usd_id(*db).amount(4) / core.amount(5);
      publish_feed(bit_usd_id(*db), witness2_id(*db), feed);
      feed.settlement_price = bit_usd_id(*db).amount(3) / core.amount(5);
      publish_feed(bit_usd_id(*db), witness3_id(*db), feed);

      // But the one from witness0 is never removed
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 4u);

      // Feed from witness1 is also expired but never deleted
      // All feeds should be deleted at this point
      const auto minimum_feeds = bit_usd_id(*db).bitasset_data(*db).options.minimum_feeds;
      BOOST_CHECK_EQUAL(minimum_feeds, 1u);

      // Advancing to next maint
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      //  All expired feeds are deleted
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 0u);

      // witness1 start feed producing again
      feed.settlement_price = bit_usd_id(*db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(*db), witness1_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      itr = bitasset_data.feeds.begin();

      // generate some blocks up to expiration but feed will not be deleted yet as need next maint time
      generate_blocks(itr[0].second.first + feed_lifetime + 1);

      // add another feed with witness2
      feed.settlement_price = bit_usd_id(*db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(*db), witness2_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();

      // make the first feed expire
      generate_blocks(itr[0].second.first + feed_lifetime + 1);
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      // feed from witness0 expires and gets deleted, feed from witness is on time so persist
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      itr = bitasset_data.feeds.begin();

      // expire everything
      generate_blocks(itr[0].second.first + feed_lifetime + 1);
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 0u);

      // add new feed with witness1
      feed.settlement_price = bit_usd_id(*db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(*db), witness1_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      itr = bitasset_data.feeds.begin();

      // Reactivate witness0
      transfer(committee_account, witness0_id, asset(1000));
      set_expiration(db.get(), trx);
      {
         account_update_operation op;
         op.account = witness0_id;
         op.new_options = witness0_id(*db).options;
         op.new_options->votes.insert(witness0_witness_id(*db).vote_id);
      
         trx.operations.push_back(op);
         sign(trx, witness0_private_key);
         PUSH_TX(db.get(), trx);
         trx.clear();
      }

      // This will deactivate witness1 as it is the one with less votes
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      // Checking
      witnesses = db->get_global_properties().active_witnesses;
      // feed from witness1 is still here as the witness is no longer a producer but the feed is not yet expired
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      itr = bitasset_data.feeds.begin();

      // make feed from witness1 expire
      generate_blocks(itr[0].second.first + feed_lifetime + 1);
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 0u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(bsip36_update_feed_producers)
{
   try
   {
      /* For MPA fed by non witnesses or non committee mmembers but by feed producers changes should do nothing */
      ACTORS( (sam)(alice)(paul)(bob) );

      // Create the asset
      const asset_id_type bit_usd_id = create_bitasset("USDBIT").id;

      // Update asset issuer
      const asset_object &asset_obj = bit_usd_id(*db);
      {
         asset_update_operation op;
         op.asset_to_update = bit_usd_id;
         op.issuer = asset_obj.issuer;
         op.new_issuer = bob_id;
         op.new_options = asset_obj.options;
         op.new_options.flags &= ~witness_fed_asset;
         trx.operations.push_back(op);
         PUSH_TX(db.get(), trx, ~0);
         generate_block();
         trx.clear();
      }

      // Add 3 feed producers for asset
      {
         asset_update_feed_producers_operation op;
         op.asset_to_update = bit_usd_id;
         op.issuer = bob_id;
         op.new_feed_producers = {sam_id, alice_id, paul_id};
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         PUSH_TX(db.get(), trx);
         generate_block();
         trx.clear();
      }

      // Bitshares will create entries in the field feed after feed producers are added
      auto bitasset_data = bit_usd_id(*db).bitasset_data(*db);

      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      auto itr = bitasset_data.feeds.begin();

      // Removing a feed producer
      {
         asset_update_feed_producers_operation op;
         op.asset_to_update = bit_usd_id;
         op.issuer = bob_id;
         op.new_feed_producers = {alice_id, paul_id};
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         PUSH_TX(db.get(), trx);
         generate_block();
         trx.clear();
      }

      // Feed for removed producer is removed
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();

      // Feed persist after expiration
      const auto feed_lifetime = bit_usd_id(*db).bitasset_data(*db).options.feed_lifetime_sec;
      generate_blocks(db->head_block_time() + feed_lifetime + 1);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);

      // Advancing to next maint
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      // Expired feeds persist, no changes
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(bsip36_additional)
{
   try
   {
      /* Check impact of bsip36 with multiple feeds */
      INVOKE( bsip36 );

      // get the stuff needed from invoked test
      const asset_id_type bit_usd_id = get_asset("USDBIT").id;
      const asset_id_type core_id = asset_id_type();
      const account_id_type witness5_id= get_account("witness5").id;
      const account_id_type witness6_id= get_account("witness6").id;
      const account_id_type witness7_id= get_account("witness7").id;
      const account_id_type witness8_id= get_account("witness8").id;
      const account_id_type witness9_id= get_account("witness9").id;
      const account_id_type witness10_id= get_account("witness10").id;


      set_expiration( db.get(), trx );

      // changing lifetime feed to 5 days
      // maint interval default is every 1 day
      {
         asset_update_bitasset_operation op;
         op.new_options.minimum_feeds = 3;
         op.new_options.feed_lifetime_sec = 86400 * 5;
         op.asset_to_update = bit_usd_id;
         op.issuer = bit_usd_id(*db).issuer;
         trx.operations.push_back(op);
         PUSH_TX(db.get(), trx, ~0);
         generate_block();
         trx.clear();
      }

      price_feed feed;
      feed.settlement_price = bit_usd_id(*db).amount(1) / core_id(*db).amount(5);
      publish_feed(bit_usd_id(*db), witness5_id(*db), feed);
      auto bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      auto itr = bitasset_data.feeds.begin();

      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(*db).amount(1) / core_id(*db).amount(5);
      publish_feed(bit_usd_id(*db), witness6_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();

      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(*db).amount(1) / core_id(*db).amount(5);
      publish_feed(bit_usd_id(*db), witness7_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      itr = bitasset_data.feeds.begin();

      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(*db).amount(1) / core_id(*db).amount(5);
      publish_feed(bit_usd_id(*db), witness8_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 4u);
      itr = bitasset_data.feeds.begin();

      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(*db).amount(1) / core_id(*db).amount(5);
      publish_feed(bit_usd_id(*db), witness9_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 5u);
      itr = bitasset_data.feeds.begin();

      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(*db).amount(1) / core_id(*db).amount(5);
      publish_feed(bit_usd_id(*db), witness10_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 6u);
      itr = bitasset_data.feeds.begin();

      // make the older feed expire
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 5u);
      itr = bitasset_data.feeds.begin();

      // make older 2 feeds expire
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      itr = bitasset_data.feeds.begin();

      // witness5 add new feed, feeds are sorted by witness_id not by feed_time
      feed.settlement_price = bit_usd_id(*db).amount(1) / core_id(*db).amount(5);
      publish_feed(bit_usd_id(*db), witness5_id(*db), feed);
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 4u);
      itr = bitasset_data.feeds.begin();

      // another feed expires
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      itr = bitasset_data.feeds.begin();

      // another feed expires
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
      generate_block();
      bitasset_data = bit_usd_id(*db).bitasset_data(*db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();

      // and so on

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
