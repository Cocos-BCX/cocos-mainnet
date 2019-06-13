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

#include <bitset>
#include <iostream>

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;

BOOST_AUTO_TEST_SUITE(block_tests)

BOOST_FIXTURE_TEST_CASE( update_account_keys, database_fixture )
{
   try
   {
      const asset_object& core = asset_id_type()(db);
      uint32_t skip_flags =
          database::skip_transaction_dupe_check
        | database::skip_witness_signature
        | database::skip_transaction_signatures
        | database::skip_authority_check
        ;

      // Sam is the creator of accounts
      private_key_type committee_key = init_account_priv_key;
      private_key_type sam_key = generate_private_key("sam");

      //
      // A = old key set
      // B = new key set
      //
      // we measure how many times we test following four cases:
      //
      //                                    A-B        B-A
      // alice     case_count[0]   A == B    empty      empty
      // bob       case_count[1]   A  < B    empty      nonempty
      // charlie   case_count[2]   B  < A    nonempty   empty
      // dan       case_count[3]   A nc B    nonempty   nonempty
      //
      // and assert that all four cases were tested at least once
      //
      account_object sam_account_object = create_account( "sam", sam_key );

      //Get a sane head block time
      generate_block( skip_flags );

      db.modify(db.get_global_properties(), [](global_property_object& p) {
         p.parameters.committee_proposal_review_period = fc::hours(1).to_seconds();
      });

      transaction tx;
      processed_transaction ptx;

      account_object committee_account_object = committee_account(db);
      // transfer from committee account to Sam account
      transfer(committee_account_object, sam_account_object, core.amount(100000));

      const int num_keys = 5;
      vector< private_key_type > numbered_private_keys;
      vector< vector< public_key_type > > numbered_key_id;
      numbered_private_keys.reserve( num_keys );
      numbered_key_id.push_back( vector<public_key_type>() );
      numbered_key_id.push_back( vector<public_key_type>() );

      for( int i=0; i<num_keys; i++ )
      {
         private_key_type privkey = generate_private_key(
            std::string("key_") + std::to_string(i));
         public_key_type pubkey = privkey.get_public_key();
         address addr( pubkey );

         numbered_private_keys.push_back( privkey );
         numbered_key_id[0].push_back( pubkey );
         //numbered_key_id[1].push_back( addr );
      }

      // each element of possible_key_sched is a list of exactly num_keys
      // indices into numbered_key_id[use_address].  they are defined
      // by repeating selected elements of
      // numbered_private_keys given by a different selector.
      vector< vector< int > > possible_key_sched;
      const int num_key_sched = (1 << num_keys)-1;
      possible_key_sched.reserve( num_key_sched );

      for( int s=1; s<=num_key_sched; s++ )
      {
         vector< int > v;
         int i = 0;
         v.reserve( num_keys );
         while( v.size() < num_keys )
         {
            if( s & (1 << i) )
               v.push_back( i );
            i++;
            if( i >= num_keys )
               i = 0;
         }
         possible_key_sched.push_back( v );
      }

      // we can only undo in blocks
      generate_block( skip_flags );

      std::cout << "update_account_keys:  this test will take a few minutes...\n";
      for( int use_addresses=0; use_addresses<2; use_addresses++ )
      {
         vector< public_key_type > key_ids = numbered_key_id[ use_addresses ];
         for( int num_owner_keys=1; num_owner_keys<=2; num_owner_keys++ )
         {
            for( int num_active_keys=1; num_active_keys<=2; num_active_keys++ )
            {
               std::cout << use_addresses << num_owner_keys << num_active_keys << "\n";
               for( const vector< int >& key_sched_before : possible_key_sched )
               {
                  auto it = key_sched_before.begin();
                  vector< const private_key_type* > owner_privkey;
                  vector< const public_key_type* > owner_keyid;
                  owner_privkey.reserve( num_owner_keys );

                  trx.clear();
                  account_create_operation create_op;
                  create_op.name = "alice";

                  for( int owner_index=0; owner_index<num_owner_keys; owner_index++ )
                  {
                     int i = *(it++);
                     create_op.owner.key_auths[ key_ids[ i ] ] = 1;
                     owner_privkey.push_back( &numbered_private_keys[i] );
                     owner_keyid.push_back( &key_ids[ i ] );
                  }
                  // size() < num_owner_keys is possible when some keys are duplicates
                  create_op.owner.weight_threshold = create_op.owner.key_auths.size();

                  for( int active_index=0; active_index<num_active_keys; active_index++ )
                     create_op.active.key_auths[ key_ids[ *(it++) ] ] = 1;
                  // size() < num_active_keys is possible when some keys are duplicates
                  create_op.active.weight_threshold = create_op.active.key_auths.size();

                  create_op.options.memo_key = key_ids[ *(it++) ] ;
                  create_op.registrar = sam_account_object.id;
                  trx.operations.push_back( create_op );
                  // trx.sign( sam_key );
                  wdump( (trx) );

                  processed_transaction ptx_create = db.push_transaction( trx,
                     database::skip_transaction_dupe_check |
                     database::skip_transaction_signatures |
                     database::skip_authority_check
                      );
                  account_id_type alice_account_id =
                     ptx_create.operation_results[0]
                     .get< object_id_result>().result;

                  generate_block( skip_flags );
                  for( const vector< int >& key_sched_after : possible_key_sched )
                  {
                     auto it = key_sched_after.begin();

                     trx.clear();
                     account_update_operation update_op;
                     update_op.account = alice_account_id;
                     update_op.owner = authority();
                     update_op.active = authority();
                     update_op.new_options = create_op.options;

                     for( int owner_index=0; owner_index<num_owner_keys; owner_index++ )
                        update_op.owner->key_auths[ key_ids[ *(it++) ] ] = 1;
                     // size() < num_owner_keys is possible when some keys are duplicates
                     update_op.owner->weight_threshold = update_op.owner->key_auths.size();
                     for( int active_index=0; active_index<num_active_keys; active_index++ )
                        update_op.active->key_auths[ key_ids[ *(it++) ] ] = 1;
                     // size() < num_active_keys is possible when some keys are duplicates
                     update_op.active->weight_threshold = update_op.active->key_auths.size();
                     FC_ASSERT( update_op.new_options.valid() );
                     update_op.new_options->memo_key = key_ids[ *(it++) ] ;

                     trx.operations.push_back( update_op );
                     for( int i=0; i<int(create_op.owner.weight_threshold); i++)
                     {
                        sign( trx, *owner_privkey[i] );
                        if( i < int(create_op.owner.weight_threshold-1) )
                        {
                           GRAPHENE_REQUIRE_THROW(db.push_transaction(trx), fc::exception);
                        }
                        else
                        {
                           db.push_transaction( trx,
                           database::skip_transaction_dupe_check |
                           database::skip_transaction_signatures );
                        }
                     }
                     verify_account_history_plugin_index();
                     generate_block( skip_flags );

                     verify_account_history_plugin_index();
                     db.pop_block();
                     verify_account_history_plugin_index();
                  }
                  db.pop_block();
                  verify_account_history_plugin_index();
               }
            }
         }
      }
   }
   catch( const fc::exception& e )
   {
      edump( (e.to_detail_string()) );
      throw;
   }
}

/**
 *  To have a secure random number we need to ensure that the same
 *  witness does not get to produce two blocks in a row.  There is
 *  always a chance that the last witness of one round will be the
 *  first witness of the next round.
 *
 *  This means that when we shuffle witness we need to make sure
 *  that there is at least N/2 witness between consecutive turns
 *  of the same witness.    This means that durring the random
 *  shuffle we need to restrict the placement of witness to maintain
 *  this invariant.
 *
 *  This test checks the requirement using Monte Carlo approach
 *  (produce lots of blocks and check the invariant holds).
 */
BOOST_FIXTURE_TEST_CASE( witness_order_mc_test, database_fixture )
{
   try {
      size_t num_witnesses = db.get_global_properties().active_witnesses.size();
      size_t dmin = num_witnesses >> 1;

      vector< witness_id_type > cur_round;
      vector< witness_id_type > full_schedule;
      // if we make the maximum witness count testable,
      // we'll need to enlarge this.
      std::bitset< 0x40 > witness_seen;
      size_t total_blocks = 1000000;

      cur_round.reserve( num_witnesses );
      full_schedule.reserve( total_blocks );
      cur_round.push_back( db.get_dynamic_global_properties().current_witness );

      // we assert so the test doesn't continue, which would
      // corrupt memory
      assert( num_witnesses <= witness_seen.size() );

      while( full_schedule.size() < total_blocks )
      {
         if( (db.head_block_num() & 0x3FFF) == 0 )
         {
             wdump( (db.head_block_num()) );
         }
         witness_id_type wid = db.get_scheduled_witness( 1 );
         full_schedule.push_back( wid );
         cur_round.push_back( wid );
         if( cur_round.size() == num_witnesses )
         {
            // check that the current round contains exactly 1 copy
            // of each witness
            witness_seen.reset();
            for( const witness_id_type& w : cur_round )
            {
               uint64_t inst = w.instance.value;
               BOOST_CHECK( !witness_seen.test( inst ) );
               assert( !witness_seen.test( inst ) );
               witness_seen.set( inst );
            }
            cur_round.clear();
         }
         generate_block();
      }

      for( size_t i=0,m=full_schedule.size(); i<m; i++ )
      {
         for( size_t j=i+1,n=std::min( m, i+dmin ); j<n; j++ )
         {
            BOOST_CHECK( full_schedule[i] != full_schedule[j] );
            assert( full_schedule[i] != full_schedule[j] );
         }
      }

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_FIXTURE_TEST_CASE( tapos_rollover, database_fixture )
{
   try
   {
      ACTORS((alice)(bob));

      BOOST_TEST_MESSAGE( "Give Alice some money" );
      transfer(committee_account, alice_id, asset(10000));
      generate_block();

      BOOST_TEST_MESSAGE( "Generate up to block 0xFF00" );
      generate_blocks( 0xFF00 );
      signed_transaction xfer_tx;

      BOOST_TEST_MESSAGE( "Transfer money at/about 0xFF00" );
      transfer_operation xfer_op;
      xfer_op.from = alice_id;
      xfer_op.to = bob_id;
      xfer_op.amount = asset(1000);

      xfer_tx.operations.push_back( xfer_op );
      xfer_tx.set_expiration( db.head_block_time() + fc::seconds( 0x1000 * db.get_global_properties().parameters.block_interval ) );
      xfer_tx.set_reference_block( db.head_block_id() );

      sign( xfer_tx, alice_private_key );
      PUSH_TX( db, xfer_tx, 0 );
      generate_block();

      BOOST_TEST_MESSAGE( "Sign new tx's" );
      xfer_tx.set_expiration( db.head_block_time() + fc::seconds( 0x1000 * db.get_global_properties().parameters.block_interval ) );
      xfer_tx.set_reference_block( db.head_block_id() );
      xfer_tx.signatures.clear();
      sign( xfer_tx, alice_private_key );

      BOOST_TEST_MESSAGE( "Generate up to block 0x10010" );
      generate_blocks( 0x110 );

      BOOST_TEST_MESSAGE( "Transfer at/about block 0x10010 using reference block at/about 0xFF00" );
      PUSH_TX( db, xfer_tx, 0 );
      generate_block();
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE(bulk_discount, database_fixture)
{ try {
   ACTOR(nathan);
   // Give nathan ALLLLLL the money!
   transfer(GRAPHENE_COMMITTEE_ACCOUNT, nathan_id, db.get_balance(GRAPHENE_COMMITTEE_ACCOUNT, asset_id_type()));
   enable_fees();//GRAPHENE_BLOCKCHAIN_PRECISION*10);
   upgrade_to_lifetime_member(nathan_id);
   share_type new_fees;
   while( nathan_id(db).statistics(db).lifetime_fees_paid + new_fees < GRAPHENE_DEFAULT_BULK_DISCOUNT_THRESHOLD_MIN )
   {
      transfer(nathan_id, GRAPHENE_COMMITTEE_ACCOUNT, asset(1));
      new_fees += db.current_fee_schedule().calculate_fee(transfer_operation()).amount;
   }
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   enable_fees();//GRAPHENE_BLOCKCHAIN_PRECISION*10);
   auto old_cashback = nathan_id(db).cashback_balance(db).balance;

   transfer(nathan_id, GRAPHENE_COMMITTEE_ACCOUNT, asset(1));
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   enable_fees();//GRAPHENE_BLOCKCHAIN_PRECISION*10);

   BOOST_CHECK_EQUAL(nathan_id(db).cashback_balance(db).balance.amount.value,
                     old_cashback.amount.value + GRAPHENE_BLOCKCHAIN_PRECISION * 8);

   new_fees = 0;
   while( nathan_id(db).statistics(db).lifetime_fees_paid + new_fees < GRAPHENE_DEFAULT_BULK_DISCOUNT_THRESHOLD_MAX )
   {
      transfer(nathan_id, GRAPHENE_COMMITTEE_ACCOUNT, asset(1));
      new_fees += db.current_fee_schedule().calculate_fee(transfer_operation()).amount;
   }
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   enable_fees();//GRAPHENE_BLOCKCHAIN_PRECISION*10);
   old_cashback = nathan_id(db).cashback_balance(db).balance;

   transfer(nathan_id, GRAPHENE_COMMITTEE_ACCOUNT, asset(1));
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   BOOST_CHECK_EQUAL(nathan_id(db).cashback_balance(db).balance.amount.value,
                     old_cashback.amount.value + GRAPHENE_BLOCKCHAIN_PRECISION * 9);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
