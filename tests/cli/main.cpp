/*
 * Copyright (c) 2018 John Jones, and contributors.
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
#include <graphene/app/application.hpp>
#include <graphene/app/plugin.hpp>

#include <graphene/utilities/tempdir.hpp>

#include <graphene/account_history/account_history_plugin.hpp>
#include <graphene/witness/witness.hpp>
#include <graphene/market_history/market_history_plugin.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/wallet/wallet.hpp>

#include <fc/thread/thread.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/crypto/base58.hpp>

#include <fc/crypto/aes.hpp>

#ifdef _WIN32
   #ifndef _WIN32_WINNT
      #define _WIN32_WINNT 0x0501
   #endif
   #include <winsock2.h>
   #include <WS2tcpip.h>
#else
   #include <sys/socket.h>
   #include <netinet/ip.h>
   #include <sys/types.h>
#endif
#include <thread>

#include <boost/filesystem/path.hpp>

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

/*****
 * Global Initialization for Windows
 * ( sets up Winsock stuf )
 */
#ifdef _WIN32
int sockInit(void)
{
   WSADATA wsa_data;
   return WSAStartup(MAKEWORD(1,1), &wsa_data);
}
int sockQuit(void)
{
   return WSACleanup();
}
#endif

/*********************
 * Helper Methods
 *********************/

#include "../common/genesis_file_util.hpp"

#define INVOKE(test) ((struct test*)this)->test_method();

//////
/// @brief attempt to find an available port on localhost
/// @returns an available port number, or -1 on error
/////
int get_available_port()
{
   struct sockaddr_in sin;
   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (socket_fd == -1)
      return -1;
   sin.sin_family = AF_INET;
   sin.sin_port = 0;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (::bind(socket_fd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) == -1)
      return -1;
   socklen_t len = sizeof(sin);
   if (getsockname(socket_fd, (struct sockaddr *)&sin, &len) == -1)
      return -1;
#ifdef _WIN32
   closesocket(socket_fd);
#else
   close(socket_fd);
#endif
   return ntohs(sin.sin_port);
}

///////////
/// @brief Start the application
/// @param app_dir the temporary directory to use
/// @param server_port_number to be filled with the rpc endpoint port number
/// @returns the application object
//////////
std::shared_ptr<graphene::app::application> start_application(fc::temp_directory& app_dir, int& server_port_number) {
   std::shared_ptr<graphene::app::application> app1(new graphene::app::application{});

   app1->register_plugin<graphene::account_history::account_history_plugin>();
   app1->register_plugin< graphene::market_history::market_history_plugin >();
   app1->register_plugin< graphene::witness_plugin::witness_plugin >();
   app1->startup_plugins();
   boost::program_options::variables_map cfg;
#ifdef _WIN32
   sockInit();
#endif
   server_port_number = get_available_port();
   cfg.emplace(
      "rpc-endpoint",
      boost::program_options::variable_value(string("127.0.0.1:" + std::to_string(server_port_number)), false)
   );
   cfg.emplace("genesis-json", boost::program_options::variable_value(create_genesis_file(app_dir), false));
   cfg.emplace("seed-nodes", boost::program_options::variable_value(string("[]"), false));
   // app1->initialize(app_dir.path(), cfg);
   app1->initialize(cfg);

   app1->initialize_plugins(cfg);
   app1->startup_plugins();

   app1->startup();
   fc::usleep(fc::milliseconds(500));
   return app1;
}

///////////
/// Send a block to the db
/// @param app the application
/// @param returned_block the signed block
/// @returns true on success
///////////
bool generate_block(std::shared_ptr<graphene::app::application> app, graphene::chain::signed_block& returned_block) 
{
   try {
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      auto db = app->chain_database();
      returned_block = db->generate_block( db->get_slot_time(1),
                                         db->get_scheduled_witness(1),
                                         committee_key,
                                         database::skip_nothing );
      return true;
   } catch (exception &e) {
      return false;
   }
}

bool generate_block(std::shared_ptr<graphene::app::application> app)
{
   graphene::chain::signed_block returned_block;
   return generate_block(app, returned_block);
}

///////////
/// @brief Skip intermediate blocks, and generate a maintenance block
/// @param app the application
/// @returns true on success
///////////
bool generate_maintenance_block(std::shared_ptr<graphene::app::application> app) {
   try {
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      uint32_t skip = ~0;
      auto db = app->chain_database();
      auto maint_time = db->get_dynamic_global_properties().next_maintenance_time;
      auto slots_to_miss = db->get_slot_at_time(maint_time);
      db->generate_block(db->get_slot_time(slots_to_miss),
            db->get_scheduled_witness(slots_to_miss),
            committee_key,
            skip);
      return true;
   } catch (exception& e)
   {
      return false;
   }
}

///////////
/// @brief a class to make connecting to the application server easier
///////////
class client_connection
{
public:
   /////////
   // constructor
   /////////
   client_connection(
      std::shared_ptr<graphene::app::application> app,
      const fc::temp_directory& data_dir,
      const int server_port_number
   )
   {
      wallet_data.chain_id = app->chain_database()->get_chain_id();
      wallet_data.ws_server = "ws://127.0.0.1:" + std::to_string(server_port_number);
      wallet_data.ws_user = "";
      wallet_data.ws_password = "";
      websocket_connection  = websocket_client.connect( wallet_data.ws_server );

      api_connection = std::make_shared<fc::rpc::websocket_api_connection>(websocket_connection);

      remote_login_api = api_connection->get_remote_api< graphene::app::login_api >(1);
      BOOST_CHECK(remote_login_api->login( wallet_data.ws_user, wallet_data.ws_password ) );

      wallet_api_ptr = std::make_shared<graphene::wallet::wallet_api>(wallet_data, remote_login_api);
      wallet_filename = data_dir.path().generic_string() + "/wallet.json";
      wallet_api_ptr->set_wallet_filename(wallet_filename);

      wallet_api = fc::api<graphene::wallet::wallet_api>(wallet_api_ptr);

      wallet_cli = std::make_shared<fc::rpc::cli>();
      for( auto& name_formatter : wallet_api_ptr->get_result_formatters() )
         wallet_cli->format_result( name_formatter.first, name_formatter.second );

      boost::signals2::scoped_connection closed_connection(websocket_connection->closed.connect([=]{
         cerr << "Server has disconnected us.\n";
         wallet_cli->stop();
      }));
      (void)(closed_connection);
   }
   ~client_connection()
   {
      // wait for everything to finish up
      fc::usleep(fc::milliseconds(500));
   }
public:
   fc::http::websocket_client websocket_client;
   graphene::wallet::wallet_data wallet_data;
   fc::http::websocket_connection_ptr websocket_connection;
   std::shared_ptr<fc::rpc::websocket_api_connection> api_connection;
   fc::api<login_api> remote_login_api;
   std::shared_ptr<graphene::wallet::wallet_api> wallet_api_ptr;
   fc::api<graphene::wallet::wallet_api> wallet_api;
   std::shared_ptr<fc::rpc::cli> wallet_cli;
   std::string wallet_filename;
};

///////////////////////////////
// Cli Wallet Fixture
///////////////////////////////

struct cli_fixture
{
   class dummy
   {
   public:
      ~dummy()
      {
         // wait for everything to finish up
         fc::usleep(fc::milliseconds(500));
      }
   };
   dummy dmy;
   int server_port_number;
   fc::temp_directory app_dir;
   std::shared_ptr<graphene::app::application> app1;
   client_connection con;
   std::vector<std::string> nathan_keys;

   cli_fixture() :
      server_port_number(0),
      app_dir( graphene::utilities::temp_directory_path() ),
      app1( start_application(app_dir, server_port_number) ),
      con( app1, app_dir, server_port_number ),
      nathan_keys( {"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"} )
   {
      BOOST_TEST_MESSAGE("Setup cli_wallet::boost_fixture_test_case");

      using namespace graphene::chain;
      using namespace graphene::app;

      try
      {
         BOOST_TEST_MESSAGE("Setting wallet password");
         con.wallet_api_ptr->set_password("supersecret");
         con.wallet_api_ptr->unlock("supersecret");

         // import Nathan account
         BOOST_TEST_MESSAGE("Importing nathan key");
         BOOST_CHECK_EQUAL(nathan_keys[0], "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3");
         BOOST_CHECK(con.wallet_api_ptr->import_key("nathan", nathan_keys[0]));
      } catch( fc::exception& e ) {
         edump((e.to_detail_string()));
         throw;
      }
   }

   ~cli_fixture()
   {
      BOOST_TEST_MESSAGE("Cleanup cli_wallet::boost_fixture_test_case");

      // wait for everything to finish up
      fc::usleep(fc::seconds(1));

      app1->shutdown();
#ifdef _WIN32
      sockQuit();
#endif
   }
};

///////////////////////////////
// Tests
///////////////////////////////

////////////////
// Start a server and connect using the same calls as the CLI
////////////////
BOOST_FIXTURE_TEST_CASE( cli_connect, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet connection.");
}

////////////////
// Start a server and connect using the same calls as the CLI
// Quit wallet and be sure that file was saved correctly
////////////////
BOOST_FIXTURE_TEST_CASE( cli_quit, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet connection and quit command.");
   //BOOST_CHECK_THROW( con.wallet_api_ptr->quit(), fc::canceled_exception );
}

BOOST_FIXTURE_TEST_CASE( upgrade_nathan_account, cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("Upgrade Nathan's account");

      account_object nathan_acct_before_upgrade, nathan_acct_after_upgrade;
      std::vector<signed_transaction> import_txs;
      signed_transaction upgrade_tx;

      BOOST_TEST_MESSAGE("Importing nathan's balance");
      import_txs = con.wallet_api_ptr->import_balance("nathan", nathan_keys, true);
      nathan_acct_before_upgrade = con.wallet_api_ptr->get_account("nathan");

      // upgrade nathan
      BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
      auto upgrade_pair = con.wallet_api_ptr->upgrade_account("nathan", true);
      upgrade_tx = upgrade_pair.second;
      nathan_acct_after_upgrade = con.wallet_api_ptr->get_account("nathan");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
         std::not_equal_to<uint32_t>(),
         (nathan_acct_before_upgrade.membership_expiration_date.sec_since_epoch())
         (nathan_acct_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(nathan_acct_after_upgrade.is_lifetime_member());
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( create_new_account, cli_fixture )
{
   try
   {
      INVOKE(upgrade_nathan_account);

      // create a new account
      graphene::wallet::brain_key_info bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      signed_transaction create_acct_tx = con.wallet_api_ptr->create_account_with_brain_key(
         bki.brain_priv_key, "jmjatlanta", "nathan", true
      ).second;
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("jmjatlanta", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give jmjatlanta some bitsahres
      BOOST_TEST_MESSAGE("Transferring bitshares from Nathan to jmjatlanta");
      signed_transaction transfer_tx = con.wallet_api_ptr->transfer(
         "nathan", "jmjatlanta", "10000", "1.3.0",
         pair<string, bool>("Here are some CORE token for your new account", false), true
      ).second;
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////////////////
// Start a server and connect using the same calls as the CLI
// Vote for two witnesses, and make sure they both stay there
// after a maintenance block
///////////////////////
BOOST_FIXTURE_TEST_CASE( cli_vote_for_2_witnesses, cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("Cli Vote Test for 2 Witnesses");

      INVOKE(create_new_account);

      // get the details for init1
      witness_object init1_obj = con.wallet_api_ptr->get_witness("init1");
      int init1_start_votes = init1_obj.total_votes;
      // Vote for a witness
      signed_transaction vote_witness1_tx = con.wallet_api_ptr->vote_for_witness("jmjatlanta", "init1", true, true).second;

      // generate a block to get things started
      BOOST_CHECK(generate_block(app1));
      // wait for a maintenance interval
      BOOST_CHECK(generate_maintenance_block(app1));

      // Verify that the vote is there
      init1_obj = con.wallet_api_ptr->get_witness("init1");
      witness_object init2_obj = con.wallet_api_ptr->get_witness("init2");
      int init1_middle_votes = init1_obj.total_votes;
      BOOST_CHECK(init1_middle_votes > init1_start_votes);

      // Vote for a 2nd witness
      int init2_start_votes = init2_obj.total_votes;
      signed_transaction vote_witness2_tx = con.wallet_api_ptr->vote_for_witness("jmjatlanta", "init2", true, true).second;

      // send another block to trigger maintenance interval
      BOOST_CHECK(generate_maintenance_block(app1));

      // Verify that both the first vote and the 2nd are there
      init2_obj = con.wallet_api_ptr->get_witness("init2");
      init1_obj = con.wallet_api_ptr->get_witness("init1");

      int init2_middle_votes = init2_obj.total_votes;
      BOOST_CHECK(init2_middle_votes > init2_start_votes);
      int init1_last_votes = init1_obj.total_votes;
      BOOST_CHECK(init1_last_votes > init1_start_votes);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( cli_get_signed_transaction_signers, cli_fixture )
{
   try
   {
      INVOKE(upgrade_nathan_account);

      // register account and transfer funds
      const auto test_bki = con.wallet_api_ptr->suggest_brain_key();
      con.wallet_api_ptr->register_account(
         "test", test_bki.pub_key, test_bki.pub_key, "nathan", true
      );
      con.wallet_api_ptr->transfer("nathan", "test", "1000", "1.3.0", 
         std::pair<string, bool>("", false), true);

      // import key and save wallet
      BOOST_CHECK(con.wallet_api_ptr->import_key("test", test_bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // create transaction and check expected result
      auto signed_trx = con.wallet_api_ptr->transfer("test", "nathan", "10", "1.3.0",
         std::pair<string, bool>("", false), true).second;

      const auto &test_acc = con.wallet_api_ptr->get_account("test");
      flat_set<public_key_type> expected_signers = {test_bki.pub_key};
      vector<vector<account_id_type> > expected_key_refs{{test_acc.id, test_acc.id}};

      /* auto signers = con.wallet_api_ptr->get_transaction_signers(signed_trx);
      BOOST_CHECK(signers == expected_signers);

      auto key_refs = con.wallet_api_ptr->get_key_references({expected_signers.begin(), expected_signers.end()});
      BOOST_CHECK(key_refs == expected_key_refs);
      */
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( cli_get_available_transaction_signers, cli_fixture )
{
   try {
      INVOKE(upgrade_nathan_account);

      // register account
      const auto test_bki = con.wallet_api_ptr->suggest_brain_key();
      con.wallet_api_ptr->register_account( "test1", test_bki.pub_key, test_bki.pub_key, "nathan", true);
      const auto &test_acc = con.wallet_api_ptr->get_account("test");

      // create and sign transaction
      signed_transaction trx;
      trx.operations = {transfer_operation()};

      // sign with test key
      const auto test_privkey = wif_to_key( test_bki.wif_priv_key );
      BOOST_REQUIRE( test_privkey );
      trx.sign( *test_privkey, con.wallet_data.chain_id );

      // sign with other keys
      const auto privkey_1 = fc::ecc::private_key::generate();
      trx.sign( privkey_1, con.wallet_data.chain_id );

      const auto privkey_2 = fc::ecc::private_key::generate();
      trx.sign( privkey_2, con.wallet_data.chain_id );

      // verify expected result
      flat_set<public_key_type> expected_signers = {test_bki.pub_key,
                                                    privkey_1.get_public_key(),
                                                    privkey_2.get_public_key()};

      auto signers = con.wallet_api_ptr->get_signature_keys(trx);
      BOOST_CHECK(signers == expected_signers);

      // blockchain has no references to unknown accounts (privkey_1, privkey_2)
      // only test account available
      // vector<vector<account_id_type> > expected_key_refs{{}, {}, {test_acc.id, test_acc.id}};

      // auto key_refs = con.wallet_api_ptr->get_key_references({expected_signers.begin(), expected_signers.end()});
      // std::sort(key_refs.begin(), key_refs.end());

      // BOOST_CHECK(key_refs == expected_key_refs);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( cli_cant_get_signers_from_modified_transaction, cli_fixture )
{
   try {
      INVOKE(upgrade_nathan_account);

      // register account
      const auto test_bki = con.wallet_api_ptr->suggest_brain_key();
      con.wallet_api_ptr->register_account("test2", test_bki.pub_key, test_bki.pub_key, "nathan", true);

      // create and sign transaction
      signed_transaction trx;
      trx.operations = {transfer_operation()};

      // sign with test key
      const auto test_privkey = wif_to_key( test_bki.wif_priv_key );
      BOOST_REQUIRE( test_privkey );
      trx.sign( *test_privkey, con.wallet_data.chain_id );

      // modify transaction (MITM-attack)
      trx.operations.clear();

      // verify if transaction has no valid signature of test account
      flat_set<public_key_type> expected_signers_of_valid_transaction = {test_bki.pub_key};
      auto signers = con.wallet_api_ptr->get_signature_keys(trx);
      BOOST_CHECK(signers != expected_signers_of_valid_transaction);

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

/******
 * Check account history pagination (see bitshares-core/issue/1176)
 */
BOOST_FIXTURE_TEST_CASE( account_history_pagination, cli_fixture )
{
   try
   {
      // INVOKE(create_new_account);

      // attempt to give jmjatlanta some bitsahres
      BOOST_TEST_MESSAGE("Transferring bitshares from Nathan to jmjatlanta");
      // for(int i = 1; i <= 199; i++)
      // {
      //    signed_transaction transfer_tx = con.wallet_api_ptr->transfer("nathan", "jmjatlanta", std::to_string(i),
      //                                           "1.3.0", "Here are some CORE token for your new account", true).second;
      // }

      BOOST_CHECK(generate_block(app1));

      // now get account history and make sure everything is there (and no duplicates)
      std::vector<graphene::wallet::operation_detail> history = con.wallet_api_ptr->get_account_history("jmjatlanta", 300);
      BOOST_CHECK_EQUAL(201u, history.size() );

      std::set<object_id_type> operation_ids;

      for(auto& op : history)
      {
         if( operation_ids.find(op.op.id) != operation_ids.end() )
         {
            BOOST_FAIL("Duplicate found");
         }
         operation_ids.insert(op.op.id);
      }
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}


///////////////////////
// Create a multi-sig account and verify that only when all signatures are
// signed, the transaction could be broadcast
///////////////////////
// BOOST_AUTO_TEST_CASE( cli_multisig_transaction )
// {
//    using namespace graphene::chain;
//    using namespace graphene::app;
//    std::shared_ptr<graphene::app::application> app1;
//    try {
//       fc::temp_directory app_dir( graphene::utilities::temp_directory_path() );

//       int server_port_number = 0;
//       app1 = start_application(app_dir, server_port_number);

//       // connect to the server
//       client_connection con(app1, app_dir, server_port_number);

//       BOOST_TEST_MESSAGE("Setting wallet password");
//       con.wallet_api_ptr->set_password("supersecret");
//       con.wallet_api_ptr->unlock("supersecret");

//       // import Nathan account
//       BOOST_TEST_MESSAGE("Importing nathan key");
//       std::vector<std::string> nathan_keys{"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"};
//       BOOST_CHECK_EQUAL(nathan_keys[0], "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3");
//       BOOST_CHECK(con.wallet_api_ptr->import_key("nathan", nathan_keys[0]));

//       BOOST_TEST_MESSAGE("Importing nathan's balance");
//       std::vector<signed_transaction> import_txs = con.wallet_api_ptr->import_balance("nathan", nathan_keys, true);
//       account_object nathan_acct_before_upgrade = con.wallet_api_ptr->get_account("nathan");

//       // upgrade nathan
//       BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
//       signed_transaction upgrade_tx = con.wallet_api_ptr->upgrade_account("nathan", true).second;
//       account_object nathan_acct_after_upgrade = con.wallet_api_ptr->get_account("nathan");

//       // verify that the upgrade was successful
//       BOOST_CHECK_PREDICATE( std::not_equal_to<uint32_t>(), (nathan_acct_before_upgrade.membership_expiration_date.sec_since_epoch())(nathan_acct_after_upgrade.membership_expiration_date.sec_since_epoch()) );
//       BOOST_CHECK(nathan_acct_after_upgrade.is_lifetime_member());

//       // create a new multisig account
//       graphene::wallet::brain_key_info bki1 = con.wallet_api_ptr->suggest_brain_key();
//       graphene::wallet::brain_key_info bki2 = con.wallet_api_ptr->suggest_brain_key();
//       graphene::wallet::brain_key_info bki3 = con.wallet_api_ptr->suggest_brain_key();
//       graphene::wallet::brain_key_info bki4 = con.wallet_api_ptr->suggest_brain_key();
//       BOOST_CHECK(!bki1.brain_priv_key.empty());
//       BOOST_CHECK(!bki2.brain_priv_key.empty());
//       BOOST_CHECK(!bki3.brain_priv_key.empty());
//       BOOST_CHECK(!bki4.brain_priv_key.empty());

//       signed_transaction create_multisig_acct_tx;
//       account_create_operation account_create_op;

//       account_create_op.registrar = nathan_acct_after_upgrade.id;
//       account_create_op.name = "cifer.test";
//       account_create_op.owner = authority(1, bki1.pub_key, 1);
//       account_create_op.active = authority(2, bki2.pub_key, 1, bki3.pub_key, 1);
//       account_create_op.options.memo_key = bki4.pub_key;

//       create_multisig_acct_tx.operations.push_back(account_create_op);
//       con.wallet_api_ptr->sign_transaction(create_multisig_acct_tx, true);

//       // attempt to give cifer.test some bitsahres
//       BOOST_TEST_MESSAGE("Transferring bitshares from Nathan to cifer.test");
//       signed_transaction transfer_tx1 = con.wallet_api_ptr->transfer("nathan", "cifer.test", "10000",
//                "1.3.0", pair<string, bool>("Here are some BTS for your new account", false), true).second;

//       // transfer bts from cifer.test to nathan
//       BOOST_TEST_MESSAGE("Transferring bitshares from cifer.test to nathan");
//       auto dyn_props = app1->chain_database()->get_dynamic_global_properties();
//       account_object cifer_test = con.wallet_api_ptr->get_account("cifer.test");

//       // construct a transfer transaction
//       signed_transaction transfer_tx2;
//       transfer_operation xfer_op;
//       xfer_op.from = cifer_test.id;
//       xfer_op.to = nathan_acct_after_upgrade.id;
//       xfer_op.amount = asset(100000000);
//       transfer_tx2.operations.push_back(xfer_op);

//       // case1: sign a transaction without TaPoS and expiration fields
//       // expect: return a transaction with TaPoS and expiration filled
//       transfer_tx2 = con.wallet_api_ptr->sign_transaction( transfer_tx2, false );
//       BOOST_CHECK( ( transfer_tx2.ref_block_num != 0 &&
//                      transfer_tx2.ref_block_prefix != 0 ) ||
//                    ( transfer_tx2.expiration != fc::time_point_sec() ) );

//       // case2: broadcast without signature
//       // expect: exception with missing active authority
//       BOOST_CHECK_THROW(con.wallet_api_ptr->broadcast_transaction(transfer_tx2), fc::exception);

//       // case3:
//       // import one of the private keys for this new account in the wallet file,
//       // sign and broadcast with partial signatures
//       //
//       // expect: exception with missing active authority
//       BOOST_CHECK(con.wallet_api_ptr->import_key("cifer.test", bki2.wif_priv_key));
//       BOOST_CHECK_THROW(con.wallet_api_ptr->sign_transaction(transfer_tx2, true), fc::exception);

//       // case4: sign again as signature exists
//       // expect: num of signatures not increase
//       transfer_tx2 = con.wallet_api_ptr->sign_transaction(transfer_tx2, false);
//       BOOST_CHECK_EQUAL(transfer_tx2.signatures.size(), 1);

//       // case5:
//       // import another private key, sign and broadcast without full signatures
//       //
//       // expect: transaction broadcast successfully
//       BOOST_CHECK(con.wallet_api_ptr->import_key("cifer.test", bki3.wif_priv_key));
//       con.wallet_api_ptr->sign_transaction(transfer_tx2, true);
//       auto balances = con.wallet_api_ptr->list_account_balances( "cifer.test" );
//       for (auto b : balances) {
//          if (b.asset_id == asset_id_type()) {
//             BOOST_ASSERT(b == asset(900000000 - 3000000));
//          }
//       }

//       // wait for everything to finish up
//       fc::usleep(fc::seconds(1));
//    } catch( fc::exception& e ) {
//       edump((e.to_detail_string()));
//       throw;
//    }
//    app1->shutdown();
// }

graphene::wallet::plain_keys decrypt_keys( const std::string& password, const vector<char>& cipher_keys )
{
   auto pw = fc::sha512::hash( password.c_str(), password.size() );
   vector<char> decrypted = fc::aes_decrypt( pw, cipher_keys );
   return fc::raw::unpack<graphene::wallet::plain_keys>( decrypted );
}

// BOOST_AUTO_TEST_CASE( saving_keys_wallet_test ) {
//    cli_fixture cli;

//    cli.con.wallet_api_ptr->import_balance( "nathan", cli.nathan_keys, true );
//    cli.con.wallet_api_ptr->upgrade_account( "nathan", true );
//    std::string brain_key( "FICTIVE WEARY MINIBUS LENS HAWKIE MAIDISH MINTY GLYPH GYTE KNOT COCKSHY LENTIGO PROPS BIFORM KHUTBAH BRAZIL" );
//    cli.con.wallet_api_ptr->create_account_with_brain_key( brain_key, "account1", "nathan", true );

//    BOOST_CHECK_NO_THROW( cli.con.wallet_api_ptr->transfer( "nathan", "account1", "9000", "1.3.0", pair<string, bool>("", false), true ) );

//    std::string path( cli.app_dir.path().generic_string() + "/wallet.json" );
//    graphene::wallet::wallet_data wallet = fc::json::from_file( path ).as<graphene::wallet::wallet_data>( 2 * 100 );
//    BOOST_CHECK( wallet.extra_keys.size() == 1 ); // nathan
//    BOOST_CHECK( wallet.pending_account_registrations.size() == 1 ); // account1
//    BOOST_CHECK( wallet.pending_account_registrations["account1"].size() == 2 ); // account1 active key + account1 memo key

//    graphene::wallet::plain_keys pk = decrypt_keys( "supersecret", wallet.cipher_keys );
//    BOOST_CHECK( pk.keys.size() == 1 ); // nathan key

//    BOOST_CHECK( generate_block( cli.app1 ) );
//    fc::usleep( fc::seconds(1) );

//    wallet = fc::json::from_file( path ).as<graphene::wallet::wallet_data>( 2 * 100 );
//    BOOST_CHECK( wallet.extra_keys.size() == 2 ); // nathan + account1
//    BOOST_CHECK( wallet.pending_account_registrations.empty() );
//    BOOST_CHECK_NO_THROW( cli.con.wallet_api_ptr->transfer( "account1", "nathan", "1000", "1.3.0", pair<string, bool>("", false), true ) );

//    pk = decrypt_keys( "supersecret", wallet.cipher_keys );
//    BOOST_CHECK( pk.keys.size() == 3 ); // nathan key + account1 active key + account1 memo key
// }

