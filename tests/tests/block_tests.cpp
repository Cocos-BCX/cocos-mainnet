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
#include <graphene/chain/exceptions.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/market_object.hpp>

#include <graphene/utilities/tempdir.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

genesis_state_type make_genesis() 
{
   genesis_state_type genesis_state;

   genesis_state.initial_timestamp = time_point_sec( GRAPHENE_TESTING_GENESIS_TIMESTAMP );

   auto init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));
   genesis_state.initial_active_witnesses = 11;
   for( unsigned int i = 0; i < genesis_state.initial_active_witnesses; ++i )
   {
      auto name = "init"+fc::to_string(i);
      genesis_state.initial_accounts.emplace_back(name,
                                                  init_account_priv_key.get_public_key(),
                                                  init_account_priv_key.get_public_key(),
                                                  true);
      genesis_state.initial_committee_candidates.push_back({name});
      genesis_state.initial_witness_candidates.push_back({name, init_account_priv_key.get_public_key()});
   }
   genesis_state.initial_parameters.current_fees->zero_all_fees();
   return genesis_state;
}

BOOST_AUTO_TEST_SUITE(block_tests)

BOOST_AUTO_TEST_CASE( block_database_test )
{
   try {
      fc::temp_directory data_dir( graphene::utilities::temp_directory_path() );
      ilog("data_dir: " + data_dir.path().string());
      block_database bdb;
      bdb.open( data_dir.path() );
      FC_ASSERT( bdb.is_open() );
      bdb.close();
      FC_ASSERT( !bdb.is_open() );
      bdb.open( data_dir.path() );

      signed_block b;
      for( uint32_t i = 0; i < 5; ++i )
      {
         if( i > 0 ) 
         {
            b.previous = b.make_id();
         }
         b.witness = witness_id_type(i+1);
         bdb.store( b.make_id(), b );

         auto fetch = bdb.fetch_by_number( b.block_num() );
         FC_ASSERT( fetch.valid() );
         FC_ASSERT( fetch->witness ==  b.witness );

         fetch = bdb.fetch_by_number( i+1 );
         FC_ASSERT( fetch.valid() );
         FC_ASSERT( fetch->witness ==  b.witness );

         fetch = bdb.fetch_optional( b.make_id() );
         FC_ASSERT( fetch.valid() );
         FC_ASSERT( fetch->witness ==  b.witness );
      }

      for( uint32_t i = 1; i < 5; ++i )
      {
         auto blk = bdb.fetch_by_number( i );
         FC_ASSERT( blk.valid() );
         FC_ASSERT( blk->witness == witness_id_type(blk->block_num()) );
      }

      auto last = bdb.last();
      FC_ASSERT( last );
      FC_ASSERT( last->make_id() == b.make_id() );

      bdb.close();
      bdb.open( data_dir.path() );
      last = bdb.last();
      FC_ASSERT( last );
      FC_ASSERT( last->make_id() == b.make_id() );

      for( uint32_t i = 0; i < 5; ++i )
      {
         auto blk = bdb.fetch_by_number( i+1 );
         FC_ASSERT( blk.valid() );
         FC_ASSERT( blk->witness == witness_id_type(blk->block_num()) );
      }

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( generate_empty_blocks )
{
   try {
      fc::time_point_sec now( GRAPHENE_TESTING_GENESIS_TIMESTAMP );
      fc::temp_directory data_dir( graphene::utilities::temp_directory_path() );
      signed_block b;

      // TODO:  Don't generate this here
      auto init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
      signed_block cutoff_block;
      uint32_t last_block;

      {
         database db(data_dir.path());
         db.open(data_dir.path(), make_genesis, "TEST" );
         b = db.generate_block(db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);

         // TODO:  Change this test when we correct #406
         // n.b. we generate GRAPHENE_MIN_UNDO_HISTORY+1 extra blocks which will be discarded on save
         for( uint32_t i = 1; ; ++i )
         {
            BOOST_CHECK( db.head_block_id() == b.make_id() );
            //witness_id_type prev_witness = b.witness;
            witness_id_type cur_witness = db.get_scheduled_witness(1);
            //BOOST_CHECK( cur_witness != prev_witness );
            b = db.generate_block(db.get_slot_time(1), cur_witness, init_account_priv_key, database::skip_nothing);
            BOOST_CHECK( b.witness == cur_witness );
            uint32_t cutoff_height = db.get_dynamic_global_properties().last_irreversible_block_num;
            if( cutoff_height >= 200 )
            {
               cutoff_block = *(db.fetch_block_by_number( cutoff_height ));
               last_block = db.head_block_num();
               break;
            }
         }
         db.close();
      }

      {
        database db(data_dir.path());
         db.open(data_dir.path(), []{return genesis_state_type();}, "TEST");
         BOOST_CHECK_EQUAL( db.head_block_num(), last_block );
         while( db.head_block_num() > cutoff_block.block_num() )
            db.pop_block();
         b = cutoff_block;
         for( uint32_t i = 0; i < 200; ++i )
         {
            BOOST_CHECK( db.head_block_id() == b.make_id() );
            //witness_id_type prev_witness = b.witness;
            witness_id_type cur_witness = db.get_scheduled_witness(1);
            //BOOST_CHECK( cur_witness != prev_witness );
            b = db.generate_block(db.get_slot_time(1), cur_witness, init_account_priv_key, database::skip_nothing);
         }
         BOOST_CHECK_EQUAL( db.head_block_num(), cutoff_block.block_num()+200 );
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( undo_block )
{
   try {
      fc::temp_directory data_dir( graphene::utilities::temp_directory_path() );
      {
        database db(data_dir.path());
         db.open(data_dir.path(), make_genesis, "TEST");
         fc::time_point_sec now( GRAPHENE_TESTING_GENESIS_TIMESTAMP );
         std::vector< time_point_sec > time_stack;

         auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
         for( uint32_t i = 0; i < 5; ++i )
         {
            now = db.get_slot_time(1);
            time_stack.push_back( now );
            auto b = db.generate_block( now, db.get_scheduled_witness( 1 ), init_account_priv_key, database::skip_nothing );
         }
         BOOST_CHECK( db.head_block_num() == 5 );
         BOOST_CHECK( db.head_block_time() == now );
         db.pop_block();
         time_stack.pop_back();
         now = time_stack.back();
         BOOST_CHECK( db.head_block_num() == 4 );
         BOOST_CHECK( db.head_block_time() == now );
         db.pop_block();
         time_stack.pop_back();
         now = time_stack.back();
         BOOST_CHECK( db.head_block_num() == 3 );
         BOOST_CHECK( db.head_block_time() == now );
         db.pop_block();
         time_stack.pop_back();
         now = time_stack.back();
         BOOST_CHECK( db.head_block_num() == 2 );
         BOOST_CHECK( db.head_block_time() == now );
         for( uint32_t i = 0; i < 5; ++i )
         {
            now = db.get_slot_time(1);
            time_stack.push_back( now );
            auto b = db.generate_block( now, db.get_scheduled_witness( 1 ), init_account_priv_key, database::skip_nothing );
         }
         BOOST_CHECK( db.head_block_num() == 7 );
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( fork_blocks )
{
   try {
      fc::temp_directory data_dir1( graphene::utilities::temp_directory_path() );
      fc::temp_directory data_dir2( graphene::utilities::temp_directory_path() );

      database db1(data_dir1.path());
      db1.open(data_dir1.path(), make_genesis, "TEST");
      database db2(data_dir2.path());
      db2.open(data_dir2.path(), make_genesis, "TEST");
      BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );

      auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
      for( uint32_t i = 0; i < 10; ++i )
      {
         auto b = db1.generate_block(db1.get_slot_time(1), db1.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);
         try {
            PUSH_BLOCK( &db2, b );
         } FC_CAPTURE_AND_RETHROW( ("db2") );
      }
      for( uint32_t i = 10; i < 13; ++i )
      {
         auto b =  db1.generate_block(db1.get_slot_time(1), db1.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);
      }
      string db1_tip = db1.head_block_id().str();
      uint32_t next_slot = 3;
      for( uint32_t i = 13; i < 16; ++i )
      {
         auto b =  db2.generate_block(db2.get_slot_time(next_slot), db2.get_scheduled_witness(next_slot), init_account_priv_key, database::skip_nothing);
         next_slot = 1;
         // notify both databases of the new block.
         // only db2 should switch to the new fork, db1 should not
         PUSH_BLOCK( &db1, b );
         BOOST_CHECK_EQUAL(db1.head_block_id().str(), db1_tip);
         BOOST_CHECK_EQUAL(db2.head_block_id().str(), b.make_id().str());
      }

      //The two databases are on distinct forks now, but at the same height. Make a block on db2, make it invalid, then
      //pass it to db1 and assert that db1 doesn't switch to the new fork.
      signed_block good_block;
      BOOST_CHECK_EQUAL(db1.head_block_num(), 13);
      BOOST_CHECK_EQUAL(db2.head_block_num(), 13);
      {
         auto b = db2.generate_block(db2.get_slot_time(1), db2.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);
         good_block = b;
         auto tx=signed_transaction();
         b.transactions.emplace_back(std::make_pair(tx.hash(),tx));
         b.transactions.back().second.operations.emplace_back(transfer_operation());
         b.sign( init_account_priv_key );
         BOOST_CHECK_EQUAL(b.block_num(), 14);
         GRAPHENE_CHECK_THROW(PUSH_BLOCK( &db1, b ), fc::exception);
      }
      BOOST_CHECK_EQUAL(db1.head_block_num(), 13);
      BOOST_CHECK_EQUAL(db1.head_block_id().str(), db1_tip);

      // assert that db1 switches to new fork with good block
      BOOST_CHECK_EQUAL(db2.head_block_num(), 14);
      PUSH_BLOCK( &db1, good_block );
      BOOST_CHECK_EQUAL(db1.head_block_id().str(), db2.head_block_id().str());
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( undo_pending )
{
   try {
      fc::temp_directory data_dir( graphene::utilities::temp_directory_path() );
      {
         database db(data_dir.path());
         db.open(data_dir.path(), make_genesis, "TEST");

         auto init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
         public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();
         const graphene::db::index& account_idx = db.get_index(protocol_ids, account_object_type);

         transfer_operation t;
         t.to = account_id_type(1);
         t.amount = asset( 10000000 );
         {
            signed_transaction trx;
            set_expiration( &db, trx );

            trx.operations.push_back(t);
            PUSH_TX( &db, trx, ~0 );

            auto b = db.generate_block(db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, ~0);
         }

         signed_transaction trx;
         set_expiration( &db, trx );
         account_id_type nathan_id = account_idx.get_next_id();
         account_create_operation cop;
         cop.registrar = GRAPHENE_TEMP_ACCOUNT;
         cop.name = "nathan";
         cop.owner = authority(1, init_account_pub_key, 1);
         cop.active = cop.owner;
         trx.operations.push_back(cop);
         //sign( trx,  init_account_priv_key  );
         PUSH_TX( &db, trx );

         auto b = db.generate_block(db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);

         BOOST_CHECK(nathan_id(db).name == "nathan");

         trx.clear();
         set_expiration( &db, trx );
         t.from = account_id_type(1);
         t.to = nathan_id;
         t.amount = asset(5000);
         trx.operations.push_back(t);
         db.push_transaction(trx, ~0);
         trx.clear();
         set_expiration( &db, trx );
         trx.operations.push_back(t);
         db.push_transaction(trx, ~0);

         BOOST_CHECK(db.get_balance(nathan_id, asset_id_type()).amount == 10000);
         db.clear_pending();
         BOOST_CHECK(db.get_balance(nathan_id, asset_id_type()).amount == 0);
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( switch_forks_undo_create )
{
   try {
      fc::temp_directory dir1( graphene::utilities::temp_directory_path() ),
                         dir2( graphene::utilities::temp_directory_path() );
      database db1(dir1.path());
      database db2(dir2.path());
      db1.open(dir1.path(), make_genesis, "TEST");
      db2.open(dir2.path(), make_genesis, "TEST");
      BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );

      auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
      public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();
      const graphene::db::index& account_idx = db1.get_index(protocol_ids, account_object_type);

      signed_transaction trx;
      set_expiration( &db1, trx );
      account_id_type nathan_id = account_idx.get_next_id();
      account_create_operation cop;
      cop.registrar = GRAPHENE_TEMP_ACCOUNT;
      cop.name = "nathan";
      cop.owner = authority(1, init_account_pub_key, 1);
      cop.active = cop.owner;
      trx.operations.push_back(cop);
      PUSH_TX( &db1, trx );

      // generate blocks
      // db1 : A
      // db2 : B C D

      auto aw = db1.get_global_properties().active_witnesses;
      auto b = db1.generate_block(db1.get_slot_time(1), db1.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);

      BOOST_CHECK(nathan_id(db1).name == "nathan");

      b = db2.generate_block(db2.get_slot_time(1), db2.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);
      db1.push_block(b);
      aw = db2.get_global_properties().active_witnesses;
      b = db2.generate_block(db2.get_slot_time(1), db2.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);
      db1.push_block(b);
      // GRAPHENE_REQUIRE_THROW(nathan_id(db2), fc::exception);  // not found, fc::exception?
      // nathan_id(db1); /// it should be included in the pending state, but not found
      db1.clear_pending(); // clear it so that we can verify it was properly removed from pending state.
      GRAPHENE_REQUIRE_THROW(nathan_id(db1), fc::exception);

      PUSH_TX( &db2, trx );

      aw = db2.get_global_properties().active_witnesses;
      b = db2.generate_block(db2.get_slot_time(1), db2.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);
      db1.push_block(b);

      BOOST_CHECK(nathan_id(db1).name == "nathan");
      BOOST_CHECK(nathan_id(db2).name == "nathan");
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( duplicate_transactions )
{
   try {
      fc::temp_directory dir1( graphene::utilities::temp_directory_path() ),
                         dir2( graphene::utilities::temp_directory_path() );
      database db1(dir1.path());
      database db2(dir2.path());
      db1.open(dir1.path(), make_genesis, "TEST");
      db2.open(dir2.path(), make_genesis, "TEST");
      BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );

      auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;

      auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
      public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();
      const graphene::db::index& account_idx = db1.get_index(protocol_ids, account_object_type);

      signed_transaction trx;
      set_expiration( &db1, trx );
      account_id_type nathan_id = account_idx.get_next_id();
      account_create_operation cop;
      cop.name = "nathan";
      cop.owner = authority(1, init_account_pub_key, 1);
      cop.active = cop.owner;
      trx.operations.push_back(cop);
      trx.sign( init_account_priv_key, db1.get_chain_id() );
      PUSH_TX( &db1, trx, skip_sigs );

      trx = decltype(trx)();
      set_expiration( &db1, trx );
      transfer_operation t;
      t.to = nathan_id;
      t.amount = asset(500);
      trx.operations.push_back(t);
      trx.sign( init_account_priv_key, db1.get_chain_id() );
      PUSH_TX( &db1, trx, skip_sigs );

      GRAPHENE_CHECK_THROW(PUSH_TX( &db1, trx, skip_sigs ), fc::exception);

      auto b = db1.generate_block( db1.get_slot_time(1), db1.get_scheduled_witness( 1 ), init_account_priv_key, skip_sigs );
      PUSH_BLOCK( &db2, b, skip_sigs );

      GRAPHENE_CHECK_THROW(PUSH_TX( &db1, trx, skip_sigs ), fc::exception);
      GRAPHENE_CHECK_THROW(PUSH_TX( &db2, trx, skip_sigs ), fc::exception);
      BOOST_CHECK_EQUAL(db1.get_balance(nathan_id, asset_id_type()).amount.value, 500);
      BOOST_CHECK_EQUAL(db2.get_balance(nathan_id, asset_id_type()).amount.value, 500);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( tapos )
{
   try {
      fc::temp_directory dir1( graphene::utilities::temp_directory_path() );
      database db1(dir1.path());
      db1.open(dir1.path(), make_genesis, "TEST");

      const account_object& init1 = *db1.get_index_type<account_index>().indices().get<by_name>().find("init1");

      auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
      public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();
      const graphene::db::index& account_idx = db1.get_index(protocol_ids, account_object_type);

      auto b = db1.generate_block( db1.get_slot_time(1), db1.get_scheduled_witness( 1 ), init_account_priv_key, database::skip_nothing);

      signed_transaction trx;
      //This transaction must be in the next block after its reference, or it is invalid.
      trx.set_expiration( db1.head_block_time() ); //db1.get_slot_time(1) );
      trx.set_reference_block( db1.head_block_id() );

      account_id_type nathan_id = account_idx.get_next_id();
      account_create_operation cop;
      cop.registrar = init1.id;
      cop.name = "nathan";
      cop.owner = authority(1, init_account_pub_key, 1);
      cop.active = cop.owner;
      trx.operations.push_back(cop);
      trx.sign( init_account_priv_key, db1.get_chain_id() );
      db1.push_transaction(trx);
      b = db1.generate_block(db1.get_slot_time(1), db1.get_scheduled_witness(1), init_account_priv_key, database::skip_nothing);
      trx.clear();

      transfer_operation t;
      t.to = nathan_id;
      t.amount = asset(50);
      trx.operations.push_back(t);
      trx.sign( init_account_priv_key, db1.get_chain_id() );
      //relative_expiration is 1, but ref block is 2 blocks old, so this should fail.
      GRAPHENE_REQUIRE_THROW(PUSH_TX( &db1, trx, database::skip_transaction_signatures | database::skip_authority_check ), fc::exception);
      set_expiration( &db1, trx );
      trx.signatures.clear();
      trx.sign( init_account_priv_key, db1.get_chain_id() );
      db1.push_transaction(trx, database::skip_transaction_signatures | database::skip_authority_check);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( optional_tapos, database_fixture )
{
   try
   {
      ACTORS( (antelope)(zebras) );

      generate_block();

      BOOST_TEST_MESSAGE( "Create transaction" );

      transfer( account_id_type(), antelope_id, asset( 1000000 ) );
      transfer_operation op;
      op.from = antelope_id;
      op.to = zebras_id;
      op.amount = asset( 1000 );
      signed_transaction tx;
      tx.operations.push_back( op );
      set_expiration( db.get(), tx );

      BOOST_TEST_MESSAGE( "ref_block_num=0, ref_block_prefix=0" );

      tx.ref_block_num = 0;
      tx.ref_block_prefix = 0;
      tx.signatures.clear();
      sign( tx, antelope_private_key );
      PUSH_TX( db.get(), tx );

      BOOST_TEST_MESSAGE( "proper ref_block_num, ref_block_prefix" );

      set_expiration( db.get(), tx );
      tx.signatures.clear();
      sign( tx, antelope_private_key );
      PUSH_TX( db.get(), tx );

      BOOST_TEST_MESSAGE( "ref_block_num=0, ref_block_prefix=12345678" );

      tx.ref_block_num = 0;
      tx.ref_block_prefix = 0x12345678;
      tx.signatures.clear();
      sign( tx, antelope_private_key );
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db.get(), tx ), fc::exception );

      BOOST_TEST_MESSAGE( "ref_block_num=1, ref_block_prefix=12345678" );

      tx.ref_block_num = 1;
      tx.ref_block_prefix = 0x12345678;
      tx.signatures.clear();
      sign( tx, antelope_private_key );
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db.get(), tx ), fc::exception );

      BOOST_TEST_MESSAGE( "ref_block_num=9999, ref_block_prefix=12345678" );

      tx.ref_block_num = 9999;
      tx.ref_block_prefix = 0x12345678;
      tx.signatures.clear();
      sign( tx, antelope_private_key );
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db.get(), tx ), fc::exception );
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

/*
   libraries/chain/db_maint.cpp
   FC_ASSERT(unsuccessful_candidate_vote != 0, "have not find unsuccessful candidates votes");

   run exception:
   >>> ./tests/chain_test -t block_tests/maintenance_interval
   1159158ms th_a       db_validate_block.cpp:99      _validate_block      ] Failed to push new block:
10 assert_exception: Assert Exception
unsuccessful_candidate_vote != 0: have not find unsuccessful candidates votes
    {}
    th_a  db_maint.cpp:214 pay_candidates

    {}
    th_a  db_maint.cpp:488 process_budget
1159159ms th_a       block_tests.cpp:642           test_method          ] e.to_detail_string(): 10 assert_exception: Assert Exception
unsuccessful_candidate_vote != 0: have not find unsuccessful candidates votes
    {}
    th_a  db_maint.cpp:214 pay_candidates

    {}
    th_a  db_maint.cpp:488 process_budget

    {"new_block":{"previous":"000000048d862c3dc4a822d07dace9215f11af6a","timestamp":"2015-05-16T00:00:00","witness":"1.6.3","transaction_merkle_root":"0000000000000000000000000000000000000000","witness_signature":"1f19a7a424c6cb98d8ed83b4a2d191420aceecdf9fd1c9e2b9c242f6a27b8b7eb463028939e667b314ee5e6eea6a22c329f836cf6d806edff969b82c94315dfc4c","block_id":"000000052ea90f2cef442e6a702128f3b84304fc","transactions":[]}}
    th_a  db_validate_block.cpp:106 _validate_block

    {"witness_id":"1.6.3"}
    th_a  db_block.cpp:499 _generate_block

    {}
    th_a  db_block.cpp:400 generate_block
 */
// BOOST_FIXTURE_TEST_CASE( maintenance_interval, database_fixture )
// {
//    try {
//       generate_block();
//       BOOST_CHECK_EQUAL(db->head_block_num(), 2);

//       fc::time_point_sec maintenence_time = db->get_dynamic_global_properties().next_maintenance_time;
//       BOOST_CHECK_GT(maintenence_time.sec_since_epoch(), db->head_block_time().sec_since_epoch());
//       auto initial_properties = db->get_global_properties();
//       const account_object& nathan = create_account("nathan");
//       upgrade_to_lifetime_member(nathan);
//       const committee_member_object nathans_committee_member = create_committee_member(nathan);
//       {
//          account_update_operation op;
//          op.account = nathan.id;
//          op.new_options = nathan.options;
//          op.new_options->votes.insert(nathans_committee_member.vote_id);
//          trx.operations.push_back(op);
//          PUSH_TX( db.get(), trx, ~0 );
//          trx.operations.clear();
//       }
//       transfer(account_id_type()(*db), nathan, asset(5000));

//       generate_blocks(maintenence_time - initial_properties.parameters.block_interval);
//       // BOOST_CHECK_EQUAL(db->get_global_properties().parameters.maximum_transaction_size,
//       //                   initial_properties.parameters.maximum_transaction_size);
//       BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().next_maintenance_time.sec_since_epoch(),
//                         db->head_block_time().sec_since_epoch() + db->get_global_properties().parameters.block_interval);
//       BOOST_CHECK(db->get_global_properties().active_witnesses == initial_properties.active_witnesses);
//       BOOST_CHECK(db->get_global_properties().active_committee_members == initial_properties.active_committee_members);

//       generate_block();

//       auto new_properties = db->get_global_properties();
//       BOOST_CHECK(new_properties.active_committee_members != initial_properties.active_committee_members);
//       BOOST_CHECK(std::find(new_properties.active_committee_members.begin(),
//                             new_properties.active_committee_members.end(), nathans_committee_member.id) !=
//                   new_properties.active_committee_members.end());
//       BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().next_maintenance_time.sec_since_epoch(),
//                         maintenence_time.sec_since_epoch() + new_properties.parameters.maintenance_interval);
//       maintenence_time = db->get_dynamic_global_properties().next_maintenance_time;
//       BOOST_CHECK_GT(maintenence_time.sec_since_epoch(), db->head_block_time().sec_since_epoch());
//       db->close();
//    } catch (fc::exception& e) {
//       edump((e.to_detail_string()));
//       throw;
//    }
// }


BOOST_FIXTURE_TEST_CASE( limit_order_expiration, database_fixture )
{
   try {
      //Get a sane head block time
      generate_block();

      auto* test = &create_bitasset("MIATEST");
      auto* core = &asset_id_type()(*db);
      auto* nathan = &create_account("nathan");
      auto* committee = &account_id_type()(*db);

      transfer(*committee, *nathan, core->amount(50000));

      BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 50000 );

      limit_order_create_operation op;
      op.seller = nathan->id;
      op.amount_to_sell = core->amount(500);
      op.min_to_receive = test->amount(500);
      op.expiration = db->head_block_time() + fc::seconds(10);
      trx.operations.push_back(op);
      auto ptrx = PUSH_TX( db.get(), trx, ~0 );

      BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 49500 );

      auto ptrx_id = ptrx.operation_results.back().get<object_id_result>().result;
      auto limit_index = db->get_index_type<limit_order_index>().indices();
      auto limit_itr = limit_index.begin();
      BOOST_REQUIRE( limit_itr != limit_index.end() );
      BOOST_REQUIRE( limit_itr->id == ptrx_id );
      BOOST_REQUIRE( db->find_object(limit_itr->id) );
      BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 49500 );
      auto id = limit_itr->id;

      generate_blocks(op.expiration, false);
      test = &get_asset("MIATEST");
      core = &asset_id_type()(*db);
      nathan = &get_account("nathan");
      committee = &account_id_type()(*db);

      BOOST_CHECK(db->find_object(id) == nullptr);
      BOOST_CHECK_EQUAL( get_balance(*nathan, *core), 50000 );
   } FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE( double_sign_check, database_fixture )
{
   try {
      generate_block();
      const auto& alice = account_id_type()(*db);
      ACTOR(zebras);
      asset amount(1000);

      set_expiration( db.get(), trx );
      transfer_operation t;
      t.from = alice.id;
      t.to = zebras.id;
      t.amount = amount;
      trx.operations.push_back(t);
      trx.validate();

      db->push_transaction(trx, ~0);

      trx.operations.clear();
      t.from = zebras.id;
      t.to = alice.id;
      t.amount = amount;
      trx.operations.push_back(t);
      trx.validate();

      BOOST_TEST_MESSAGE( "Verify that not-signing causes an exception" );
      GRAPHENE_REQUIRE_THROW( db->push_transaction(trx, 0), fc::exception );

      BOOST_TEST_MESSAGE( "Verify that double-signing causes an exception" );
      sign( trx, zebras_private_key );
      sign( trx, zebras_private_key );
      GRAPHENE_REQUIRE_THROW( db->push_transaction(trx, 0), tx_duplicate_sig );

      BOOST_TEST_MESSAGE( "Verify that signing with an extra, unused key fails" );
      trx.signatures.pop_back();
      sign( trx, generate_private_key("bogus" ));
      // GRAPHENE_REQUIRE_THROW( db->push_transaction(trx, 0), fc::exception );

      BOOST_TEST_MESSAGE( "Verify that signing once with the proper key passes" );
      trx.signatures.pop_back();
      db->push_transaction(trx, 0);
      sign( trx, zebras_private_key );

   } FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE( change_block_interval, database_fixture )
{ try {
   generate_block();

   db->modify(db->get_global_properties(), [](global_property_object& p) {
      p.parameters.committee_proposal_review_period = fc::hours(1).to_seconds();
   });

   BOOST_TEST_MESSAGE( "Creating a proposal to change the block_interval to 1 second" );
   {
      proposal_create_operation cop = proposal_create_operation::committee_proposal(db->get_global_properties().parameters, db->head_block_time());
      cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      cop.expiration_time = db->head_block_time() + *cop.review_period_seconds + 10;
      committee_member_update_global_parameters_operation uop;
      uop.new_parameters.block_interval = 1;
      cop.proposed_ops.emplace_back(uop);
      trx.operations.push_back(cop);
      db->push_transaction(trx);
   }
   BOOST_TEST_MESSAGE( "Updating proposal by signing with the committee_member private key" );
   {
      proposal_update_operation uop;
      uop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      uop.active_approvals_to_add = {get_account("init0").get_id(), get_account("init1").get_id(),
                                     get_account("init2").get_id(), get_account("init3").get_id(),
                                     get_account("init4").get_id(), get_account("init5").get_id(),
                                     get_account("init6").get_id(), get_account("init7").get_id()};
      trx.operations.push_back(uop);
      sign( trx, init_account_priv_key );
      /*
      sign( trx, get_account("init1" ).active.get_keys().front(),init_account_priv_key);
      sign( trx, get_account("init2" ).active.get_keys().front(),init_account_priv_key);
      sign( trx, get_account("init3" ).active.get_keys().front(),init_account_priv_key);
      sign( trx, get_account("init4" ).active.get_keys().front(),init_account_priv_key);
      sign( trx, get_account("init5" ).active.get_keys().front(),init_account_priv_key);
      sign( trx, get_account("init6" ).active.get_keys().front(),init_account_priv_key);
      sign( trx, get_account("init7" ).active.get_keys().front(),init_account_priv_key);
      */
      db->push_transaction(trx);
      BOOST_CHECK(proposal_id_type()(*db).is_authorized_to_execute(*db));
   }
   BOOST_TEST_MESSAGE( "Verifying that the interval didn't change immediately" );

   BOOST_CHECK_EQUAL(db->get_global_properties().parameters.block_interval, 5);
   auto past_time = db->head_block_time().sec_since_epoch();
   generate_block();
   BOOST_CHECK_EQUAL(db->head_block_time().sec_since_epoch() - past_time, 5);
   generate_block();
   BOOST_CHECK_EQUAL(db->head_block_time().sec_since_epoch() - past_time, 10);

   BOOST_TEST_MESSAGE( "Generating blocks until proposal expires" );
   generate_blocks(proposal_id_type()(*db).expiration_time + 5);
   BOOST_TEST_MESSAGE( "Verify that the block interval is still 5 seconds" );
   BOOST_CHECK_EQUAL(db->get_global_properties().parameters.block_interval, 5);

   BOOST_TEST_MESSAGE( "Generating blocks until next maintenance interval" );
   generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
   generate_block();   // get the maintenance skip slots out of the way

   BOOST_TEST_MESSAGE( "Verify that the new block interval is 1 second" );
   BOOST_CHECK_EQUAL(db->get_global_properties().parameters.block_interval, 5);
   past_time = db->head_block_time().sec_since_epoch();
   generate_block();
   BOOST_CHECK_EQUAL(db->head_block_time().sec_since_epoch() - past_time, 1*5);
   generate_block();
   BOOST_CHECK_EQUAL(db->head_block_time().sec_since_epoch() - past_time, 2*5);
} FC_LOG_AND_RETHROW() }

BOOST_FIXTURE_TEST_CASE( pop_block_twice, database_fixture )
{
   try
   {
      uint32_t skip_flags = (
           database::skip_witness_signature
         | database::skip_transaction_signatures
         | database::skip_authority_check
         );

      const asset_object& core = asset_id_type()(*db);

      // zebras is the creator of accounts
      private_key_type committee_key = init_account_priv_key;
      private_key_type zebras_key = generate_private_key("zebras");
      account_object zebras_account_object = create_account("zebras", zebras_key);

      //Get a sane head block time
      generate_block( skip_flags );

      db->modify(db->get_global_properties(), [](global_property_object& p) {
         p.parameters.committee_proposal_review_period = fc::hours(1).to_seconds();
      });

      transaction tx;
      processed_transaction ptx;

      account_object committee_account_object = committee_account(*db);
      // transfer from committee account to zebras account
      transfer(committee_account_object, zebras_account_object, core.amount(100000));

      generate_block(skip_flags);

      create_account("aliceaccount");
      generate_block(skip_flags);
      create_account("bobaccount");
      generate_block(skip_flags);

      db->pop_block();
      db->pop_block();
   } catch(const fc::exception& e) {
      edump( (e.to_detail_string()) );
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( rsf_missed_blocks, database_fixture )
{
   try
   {
      generate_block();

      auto rsf = [&]() -> string
      {
         fc::uint128 rsf = db->get_dynamic_global_properties().recent_slots_filled;
         string result = "";
         result.reserve(128);
         for( int i=0; i<128; i++ )
         {
            result += ((rsf.lo & 1) == 0) ? '0' : '1';
            rsf >>= 1;
         }
         return result;
      };

      auto pct = []( uint32_t x ) -> uint32_t
      {
         return uint64_t( GRAPHENE_100_PERCENT ) * x / 128;
      };

      BOOST_CHECK_EQUAL( rsf(),
         "1111111111111111111111111111111111111111111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), GRAPHENE_100_PERCENT );

      generate_block( ~0, init_account_priv_key, 1 );
      BOOST_CHECK_EQUAL( rsf(),
         "0111111111111111111111111111111111111111111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(127) );

      generate_block( ~0, init_account_priv_key, 1 );
      BOOST_CHECK_EQUAL( rsf(),
         "0101111111111111111111111111111111111111111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(126) );

      generate_block( ~0, init_account_priv_key, 2 );
      BOOST_CHECK_EQUAL( rsf(),
         "0010101111111111111111111111111111111111111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(124) );

      generate_block( ~0, init_account_priv_key, 3 );
      BOOST_CHECK_EQUAL( rsf(),
         "0001001010111111111111111111111111111111111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(121) );

      generate_block( ~0, init_account_priv_key, 5 );
      BOOST_CHECK_EQUAL( rsf(),
         "0000010001001010111111111111111111111111111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(116) );

      generate_block( ~0, init_account_priv_key, 8 );
      BOOST_CHECK_EQUAL( rsf(),
         "0000000010000010001001010111111111111111111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(108) );

      generate_block( ~0, init_account_priv_key, 13 );
      BOOST_CHECK_EQUAL( rsf(),
         "0000000000000100000000100000100010010101111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

      generate_block();
      BOOST_CHECK_EQUAL( rsf(),
         "1000000000000010000000010000010001001010111111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

      generate_block();
      BOOST_CHECK_EQUAL( rsf(),
         "1100000000000001000000001000001000100101011111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

      generate_block();
      BOOST_CHECK_EQUAL( rsf(),
         "1110000000000000100000000100000100010010101111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

      generate_block();
      BOOST_CHECK_EQUAL( rsf(),
         "1111000000000000010000000010000010001001010111111111111111111111"
         "1111111111111111111111111111111111111111111111111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(95) );

      generate_block( ~0, init_account_priv_key, 64 );
      BOOST_CHECK_EQUAL( rsf(),
         "0000000000000000000000000000000000000000000000000000000000000000"
         "1111100000000000001000000001000001000100101011111111111111111111"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(31) );

      generate_block( ~0, init_account_priv_key, 32 );
      BOOST_CHECK_EQUAL( rsf(),
         "0000000000000000000000000000000010000000000000000000000000000000"
         "0000000000000000000000000000000001111100000000000001000000001000"
      );
      BOOST_CHECK_EQUAL( db->witness_participation_rate(), pct(8) );
   }
   FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE( transaction_invalidated_in_cache, database_fixture )
{
   try
   {
      ACTORS( (antelopes)(zebras) );

      auto generate_block = [&]( graphene::chain::database* d, uint32_t skip ) -> signed_block
      {
         return d->generate_block(d->get_slot_time(1), d->get_scheduled_witness(1), init_account_priv_key, skip);
      };

      // tx's created by ACTORS() have bogus authority, so we need to
      // skip_authority_check in the block where they're included
      signed_block b1 = generate_block(db.get(), database::skip_authority_check);

      fc::temp_directory data_dir2( graphene::utilities::temp_directory_path() );

      database db2(data_dir2.path());
      db2.open(data_dir2.path(), make_genesis, "TEST");
      BOOST_CHECK( db->get_chain_id() == db2.get_chain_id() );

      while( db2.head_block_num() < db->head_block_num() )
      {
         optional< signed_block > b = db->fetch_block_by_number( db2.head_block_num()+1 );
         db2.push_block(*b, database::skip_witness_signature
                           |database::skip_authority_check );
      }
      BOOST_CHECK( db2.get( antelopes_id ).name == "antelopes" );
      BOOST_CHECK( db2.get( zebras_id ).name == "zebras" );

      db2.push_block(generate_block(db.get(), database::skip_nothing));
      transfer( account_id_type(), antelopes_id, asset( 1000 ) );
      transfer( account_id_type(),   zebras_id, asset( 1000 ) );
      // need to skip authority check here as well for same reason as above
      db2.push_block(generate_block(db.get(), database::skip_authority_check), database::skip_authority_check);

      BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value, 1000);
      BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value, 1000);
      BOOST_CHECK_EQUAL(db2.get_balance(antelopes_id, asset_id_type()).amount.value, 1000);
      BOOST_CHECK_EQUAL(db2.get_balance(  zebras_id, asset_id_type()).amount.value, 1000);

      auto generate_and_send = [&]( int n )
      {
         for( int i=0; i<n; i++ )
         {
            signed_block b = generate_block(&db2, database::skip_nothing);
            PUSH_BLOCK( db.get(), b );
         }
      };

      auto generate_xfer_tx = [&]( account_id_type from, account_id_type to, share_type amount, int blocks_to_expire ) -> signed_transaction
      {
         signed_transaction tx;
         transfer_operation xfer_op;
         xfer_op.from = from;
         xfer_op.to = to;
         xfer_op.amount = asset( amount, asset_id_type() );
         tx.operations.push_back( xfer_op );
         tx.set_expiration( db->head_block_time() + blocks_to_expire * db->get_global_properties().parameters.block_interval*10 );
         if( from == antelopes_id )
            sign( tx, antelopes_private_key );
         else
            sign( tx, zebras_private_key );
         return tx;
      };

      signed_transaction tx = generate_xfer_tx( antelopes_id, zebras_id, 1000, 10 );
      tx.set_expiration( db->head_block_time() + 10 * db->get_global_properties().parameters.block_interval );
      tx.signatures.clear();
      sign( tx, antelopes_private_key );
      // put the tx in db tx cache
      PUSH_TX( db.get(), tx );

      BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value,    0);
      BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value, 2000);

      // generate some blocks with db2, make tx expire in db's cache
      generate_and_send(3);

      // BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value, 1000);
      // BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value, 1000);

      // // generate a block with db and ensure we don't somehow apply it
      // PUSH_BLOCK(&db2, generate_block(db.get(), database::skip_nothing));
      // BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value, 1000);
      // BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value, 1000);

      // now the tricky part...
      // (A) zebras sends 1000 to antelopes
      // (B) antelopes sends 2000 to zebras
      // (C) antelopes sends 500 to zebras
      //
      // We push AB, then receive a block containing C.
      // we need to apply the block, then invalidate B in the cache.
      // AB results in antelopes having 0, zebras having 2000.
      // C results in antelopes having 500, zebras having 1500.
      //
      // This needs to occur while switching to a fork.
      //

      // signed_transaction tx_a = generate_xfer_tx( zebras_id, antelopes_id, 1000, 2 );
      // signed_transaction tx_b = generate_xfer_tx( antelopes_id, zebras_id, 2000, 10 );
      // signed_transaction tx_c = generate_xfer_tx( antelopes_id, zebras_id,  500, 10 );

      // generate_block(db.get(), database::skip_nothing );

      // PUSH_TX( db.get(), tx_a );
      // BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value, 2000);
      // BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value,    0);

      // PUSH_TX( db.get(), tx_b );
      // PUSH_TX( &db2, tx_c );

      // BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value, 0);
      // BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value, 2000);

      // BOOST_CHECK_EQUAL(db2.get_balance(antelopes_id, asset_id_type()).amount.value, 500);
      // BOOST_CHECK_EQUAL(db2.get_balance(  zebras_id, asset_id_type()).amount.value, 1500);

      // // generate enough blocks on db2 to cause db to switch forks
      // generate_and_send(2);

      // // db should invalidate B, but still be applying A, so the states don't agree

      // BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value, 1500);
      // BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value, 500);

      // BOOST_CHECK_EQUAL(db2.get_balance(antelopes_id, asset_id_type()).amount.value, 500);
      // BOOST_CHECK_EQUAL(db2.get_balance(  zebras_id, asset_id_type()).amount.value, 1500);

      // // This will cause A to expire in db
      // generate_and_send(1);

      // BOOST_CHECK_EQUAL(db->get_balance(antelopes_id, asset_id_type()).amount.value, 500);
      // BOOST_CHECK_EQUAL(db->get_balance(  zebras_id, asset_id_type()).amount.value, 1500);

      // BOOST_CHECK_EQUAL(db2.get_balance(antelopes_id, asset_id_type()).amount.value, 500);
      // BOOST_CHECK_EQUAL(db2.get_balance(  zebras_id, asset_id_type()).amount.value, 1500);

      // // Make sure we can generate and accept a plain old empty block on top of all this!
      // generate_and_send(1);
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( miss_many_blocks, database_fixture )
{
   try
   {
      generate_block();
      generate_block();
      generate_block();
      // miss 10 maintenance intervals
      generate_blocks( db->get_dynamic_global_properties().next_maintenance_time + db->get_global_properties().parameters.maintenance_interval * 10, true );
      generate_block();
      generate_block();
      generate_block();
   }
   catch (fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
