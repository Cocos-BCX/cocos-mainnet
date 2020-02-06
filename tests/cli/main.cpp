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
   app1->initialize(app_dir.path(), cfg);

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
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nico")));
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
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nicotest")));
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

      api_connection = std::make_shared<fc::rpc::websocket_api_connection>(*websocket_connection);

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
   std::vector<std::string> init0_keys;

   cli_fixture() :
      server_port_number(0),
      app_dir( graphene::utilities::temp_directory_path() ),
      app1( start_application(app_dir, server_port_number) ),
      con( app1, app_dir, server_port_number ),
      nathan_keys( {"5KAUeN3Yv51FzpLGGf4S1ByKpMqVFNzXTJK7euqc3NnaaLz1GJm"} )
   {
      BOOST_TEST_MESSAGE("Setup cli_wallet::boost_fixture_test_case");

      using namespace graphene::chain;
      using namespace graphene::app;

      try
      {
         BOOST_TEST_MESSAGE("Setting wallet password and test wallet_op");
         auto new_ret = con.wallet_api_ptr->is_new();
         auto locked_ret = con.wallet_api_ptr->is_locked();
         con.wallet_api_ptr->set_password("supersecret");
         con.wallet_api_ptr->unlock("supersecret");
         con.wallet_api_ptr->lock();
         con.wallet_api_ptr->unlock("supersecret");

         // import Nathan account
         BOOST_TEST_MESSAGE("Importing nathan key");
         BOOST_CHECK_EQUAL(nathan_keys[0], "5KAUeN3Yv51FzpLGGf4S1ByKpMqVFNzXTJK7euqc3NnaaLz1GJm");
         BOOST_CHECK(con.wallet_api_ptr->import_key("nicotest", nathan_keys[0]));
         auto dump_ret = con.wallet_api_ptr->dump_private_keys();
         BOOST_TEST_MESSAGE("Importing nathan key end");
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

BOOST_FIXTURE_TEST_CASE( general_info, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet general_info command.");
   auto help_ret = con.wallet_api_ptr->help();
   BOOST_CHECK(help_ret.length() > 0);

   auto gethelp_ret = con.wallet_api_ptr->gethelp("import_key");
   BOOST_CHECK(gethelp_ret.length() >0);

   auto info_ret = con.wallet_api_ptr->info();

   auto about_ret = con.wallet_api_ptr->about();
   auto load_ret = con.wallet_api_ptr->load_wallet_file("");
   auto normalize_ret = con.wallet_api_ptr->normalize_brain_key("SHUSH MARDY DUNGOL UNWIPED COVISIT INSULA RECART POLEAX PISH CHOEL TERMA CAUDAD   RABBLER cinCT DIMITY  ANOREXY ");
   con.wallet_api_ptr->save_wallet_file("./test.wallet");

   auto before_peers = con.wallet_api_ptr->network_get_connected_peers();
   vector<string> vec;
   //vec.push_back("127.0.0.1:8049");
   //con.wallet_api_ptr->network_add_nodes(vec);
   auto after_peers = con.wallet_api_ptr->network_get_connected_peers();
   wdump((after_peers));
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
      import_txs = con.wallet_api_ptr->import_balance("nicotest", nathan_keys, true);

      nathan_acct_before_upgrade = con.wallet_api_ptr->get_account("nicotest");

      // upgrade nathan
      BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
      auto upgrade_pair = con.wallet_api_ptr->upgrade_account("nicotest", true);
     
      upgrade_tx = upgrade_pair.second;
      BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
      nathan_acct_after_upgrade = con.wallet_api_ptr->get_account("nicotest");

      BOOST_TEST_MESSAGE("Get vesting balance");
      auto vest_balance = con.wallet_api_ptr->get_vesting_balances("nicotest");

      // verify that the upgrade was successful
      /* BOOST_CHECK_PREDICATE(
         std::not_equal_to<uint32_t>(),
         (nathan_acct_before_upgrade.membership_expiration_date.sec_since_epoch())
         (nathan_acct_after_upgrade.membership_expiration_date.sec_since_epoch())
      );wallet api is modify wrong path by cd,so this check ...
      BOOST_CHECK(nathan_acct_after_upgrade.is_lifetime_member());*/
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
         bki.brain_priv_key, "test1", "nicotest", "nicotest", true
      ).second;
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("test1", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give jmjatlanta some bitsahres
      BOOST_TEST_MESSAGE("Transferring bitshares from Nathan to jmjatlanta");
      signed_transaction transfer_tx = con.wallet_api_ptr->transfer(
         "nicotest", "test1", "10000", "1.3.0", "Here are some CORE token for your new account", true
      ).second;

      auto id = con.wallet_api_ptr->get_transaction_id(transfer_tx);

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( user_issue_asset, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet user_issue_asset command.");
   
   con.wallet_api_ptr->dbg_make_uia("nicotest","USDT");     //user issue asset
   auto vec_ret = con.wallet_api_ptr->list_assets("",5);
   BOOST_CHECK(vec_ret.size()==2);

   auto before_balance = con.wallet_api_ptr->list_account_balances("nicotest");
   BOOST_CHECK(before_balance.size()==1);

   con.wallet_api_ptr->issue_asset("nicotest","1000","USDT","issue 1000 usdt to nicotest",true);
   auto after_balance = con.wallet_api_ptr->list_account_balances("nicotest");
   BOOST_CHECK(after_balance.size()==2);
}

BOOST_FIXTURE_TEST_CASE( market_issue_asset, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet market_issue_asset command.");

   BOOST_TEST_MESSAGE("Importing nathan's balance");
   con.wallet_api_ptr->import_balance("nicotest", nathan_keys, true);
   auto after_balance = con.wallet_api_ptr->list_account_balances("nicotest");
   
   con.wallet_api_ptr->dbg_make_mia("nicotest","MSDT");     //market issue asset
   auto msdt_asset = con.wallet_api_ptr->get_asset("MSDT");

   flat_set<string> new_feed_producers;
   new_feed_producers.insert("nicotest");

   auto update_ret = con.wallet_api_ptr->update_asset_feed_producers("MSDT",new_feed_producers,true);

   asset usd(100,asset_id_type(1));
   asset core(100,asset_id_type(0));
 
   price_feed current_feed; 
   current_feed.settlement_price = msdt_asset.amount( 100 ) / core;
   current_feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different defa
   
   con.wallet_api_ptr->publish_asset_feed("nicotest","MSDT",current_feed,true);

   con.wallet_api_ptr->borrow_asset("nicotest","100","MSDT","100",true);

   con.wallet_api_ptr->sell_asset("nicotest","100","MSDT","100","COCOS",1800,false,true);

   auto balance = con.wallet_api_ptr->list_account_balances("nicotest");
   auto orders = con.wallet_api_ptr->get_limit_orders("1.3.0",std::string( msdt_asset.id ),2);
   wdump((orders));
}

BOOST_FIXTURE_TEST_CASE( committee_member, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet committee_member command.");

   INVOKE(create_new_account);

   con.wallet_api_ptr->create_committee_member("nicotest","www.test.com",true);
   con.wallet_api_ptr->get_committee_member("init0");

   auto list_committee_ret = con.wallet_api_ptr->list_committee_members("",3);
   wdump((list_committee_ret));
}

BOOST_FIXTURE_TEST_CASE( worker, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet worker command.");

   INVOKE(create_new_account);
   //variant worker_settings;
   //auto create_worker_ret = con.wallet_api_ptr->create_worker("nicotest","2019-09-12T03:00:00","2019-09-12T03:00:00",
   //                                                     "50","woker_test","www.worker.com",worker_settings,true);

   // worker_vote_delta delta;
   //auto update_worker_ret = con.wallet_api_ptr->update_worker_votes("test1",delta,true);

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

      auto witnesses = con.wallet_api_ptr->list_witnesses("",3);
      // get the details for init1
      witness_object init1_obj = con.wallet_api_ptr->get_witness("init1");
      int init1_start_votes = init1_obj.total_votes;
      auto balance = con.wallet_api_ptr->list_account_balances("test1");
      auto wallet_accounts = con.wallet_api_ptr->list_my_accounts();
      auto str = std::string(con.wallet_api_ptr->get_account("test1").id);
      auto blockchain_accounts = con.wallet_api_ptr->list_accounts("",5);

      // register account and transfer funds
      const auto test_bki = con.wallet_api_ptr->suggest_brain_key();
      con.wallet_api_ptr->register_account("test2", test_bki.pub_key, test_bki.pub_key, "nicotest", "nicotest", 0, true);
      con.wallet_api_ptr->transfer("nicotest", "test2", "1000", "1.3.0", "", true);

      // import key and save wallet
      BOOST_CHECK(con.wallet_api_ptr->import_key("test2", test_bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // Vote for a witness
      signed_transaction vote_witness1_tx = con.wallet_api_ptr->vote_for_witness("test2", "init1", true, true).second;

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
      signed_transaction vote_witness2_tx = con.wallet_api_ptr->vote_for_witness("test2", "init2", true, true).second;

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


BOOST_FIXTURE_TEST_CASE( create_witness, cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("create Witnesses");

      INVOKE(create_new_account);

      con.wallet_api_ptr->create_witness("nicotest","www.test.com",true);
      auto update_witness_ret = con.wallet_api_ptr->update_witness("nicotest","www.test.com","",true);
      wdump((update_witness_ret));
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}
///////////////////
// Start a server and connect using the same calls as the CLI
// Set a voting proxy and be assured that it sticks
///////////////////
BOOST_FIXTURE_TEST_CASE( cli_set_voting_proxy, cli_fixture )
{
   try {
      INVOKE(create_new_account);

      // grab account for comparison
      account_object prior_voting_account = con.wallet_api_ptr->get_account("nicotest");
      // set the voting proxy to nathan
      BOOST_TEST_MESSAGE("About to set voting proxy.");
      signed_transaction voting_tx = con.wallet_api_ptr->set_voting_proxy("nicotest", "test1", true).second;
      account_object after_voting_account = con.wallet_api_ptr->get_account("nicotest");
      // see if it changed
      BOOST_CHECK(prior_voting_account.options.voting_account != after_voting_account.options.voting_account);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////////////
// Test blind transactions and mantissa length of range proofs.
///////////////////
BOOST_FIXTURE_TEST_CASE( cli_confidential_tx_test, cli_fixture )
{
   using namespace graphene::wallet;
   try {
      // we need to increase the default max transaction size to run this test.
      this->app1->chain_database()->modify(
         this->app1->chain_database()->get_global_properties(), 
         []( global_property_object& p) {
            p.parameters.maximum_transaction_size = 8192;
      });      
      std::vector<signed_transaction> import_txs;

      BOOST_TEST_MESSAGE("Importing nathan's balance");
      import_txs = con.wallet_api_ptr->import_balance("nicotest", nathan_keys, true);

      unsigned int head_block = 0;
      auto & W = *con.wallet_api_ptr; // Wallet alias
      public_key_type pub_key("COCOS7yE9skpBAirth3eSNMRtwq1jYswEE3uSbbuAtXTz88HtbpQsZf");
      auto ret = W.set_key_label(pub_key,"nico_blind_label"); 
      BOOST_CHECK(ret == true);

      BOOST_TEST_MESSAGE("Creating blind accounts");
      graphene::wallet::brain_key_info bki_nathan = W.suggest_brain_key();
      graphene::wallet::brain_key_info bki_alice = W.suggest_brain_key();
      graphene::wallet::brain_key_info bki_bob = W.suggest_brain_key();
      W.create_blind_account("nicotest", bki_nathan.brain_priv_key);
      W.create_blind_account("alice", bki_alice.brain_priv_key);
      W.create_blind_account("bob", bki_bob.brain_priv_key);
      BOOST_CHECK(W.get_blind_accounts().size() == 3);

      // ** Block 1: Import Nathan account:
      BOOST_TEST_MESSAGE("Importing nathan key and balance");
      std::vector<std::string> nathan_keys{"5KAUeN3Yv51FzpLGGf4S1ByKpMqVFNzXTJK7euqc3NnaaLz1GJm"};
      W.import_key("nicotest", nathan_keys[0]);
      W.import_balance("nicotest", nathan_keys, true);
      generate_block(app1); head_block++;

      // ** Block 2: Nathan will blind 100M CORE token:
      BOOST_TEST_MESSAGE("Blinding a large balance");
      W.transfer_to_blind("nicotest", GRAPHENE_SYMBOL, {{"nicotest","100000000"}}, true);
      //W.transfer_from_blind("nicotest","test1","100", GRAPHENE_SYMBOL, true);
      BOOST_CHECK( W.get_blind_balances("nicotest")[0].amount == 10000000000000 );
      generate_block(app1); head_block++;

      // ** Block 3: Nathan will send 1M CORE token to alice and 10K CORE token to bob. We
      // then confirm that balances are received, and then analyze the range
      // prooofs to make sure the mantissa length does not reveal approximate
      // balance (issue #480).
      std::map<std::string, share_type> to_list = {{"alice",100000000000},
                                                   {"bob",    1000000000}};
      vector<blind_confirmation> bconfs;
      asset_object core_asset = W.get_asset("1.3.0");
      BOOST_TEST_MESSAGE("Sending blind transactions to alice and bob");
      for (auto to : to_list) {
         string amount = core_asset.amount_to_string(to.second);
         bconfs.push_back(W.blind_transfer("nicotest",to.first,amount,core_asset.symbol,true));
         BOOST_CHECK( W.get_blind_balances(to.first)[0].amount == to.second );
      }
      BOOST_TEST_MESSAGE("Inspecting range proof mantissa lengths");
      vector<int> rp_mantissabits;
      for (auto conf : bconfs) {
         for (auto out : conf.trx.operations[0].get<blind_transfer_operation>().outputs) {
            rp_mantissabits.push_back(1+out.range_proof[1]); // 2nd byte encodes mantissa length
         }
      }
      // We are checking the mantissa length of the range proofs for several Pedersen
      // commitments of varying magnitude.  We don't want the mantissa lengths to give
      // away magnitude.  Deprecated wallet behavior was to use "just enough" mantissa
      // bits to prove range, but this gives away value to within a factor of two. As a
      // naive test, we assume that if all mantissa lengths are equal, then they are not
      // revealing magnitude.  However, future more-sophisticated wallet behavior
      // *might* randomize mantissa length to achieve some space savings in the range
      // proof.  The following test will fail in that case and a more sophisticated test
      // will be needed.
      auto adjacent_unequal = std::adjacent_find(rp_mantissabits.begin(),
                                                 rp_mantissabits.end(),         // find unequal adjacent values
                                                 std::not_equal_to<int>());
      BOOST_CHECK(adjacent_unequal == rp_mantissabits.end());
      generate_block(app1); head_block++;

      // ** Check head block:
      BOOST_TEST_MESSAGE("Check that all expected blocks have processed");
      dynamic_global_property_object dgp = W.get_dynamic_global_properties();
      BOOST_CHECK(dgp.head_block_number == head_block);
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
      INVOKE(create_new_account);

      BOOST_TEST_MESSAGE("Transferring bitshares from nicotest to test1");
      for(int i = 1; i <= 199; i++)
      {
         signed_transaction transfer_tx = con.wallet_api_ptr->transfer("nicotest", "test1", std::to_string(i),
                                                "1.3.0", "Here are some CORE token for your new account", true).second;
      }

      BOOST_CHECK(generate_block(app1));

      // now get account history and make sure everything is there (and no duplicates)
      std::vector<graphene::wallet::operation_detail> history = con.wallet_api_ptr->get_account_history("test1", 300);

      BOOST_CHECK_EQUAL(205u, history.size() );

      auto id = con.wallet_api_ptr->get_account_id("test1");

      std::set<object_id_type> operation_ids;

      for(auto& op : history)
      {
         if( operation_ids.find(op.op.id) != operation_ids.end() )
         {
            //BOOST_FAIL("Duplicate found");
         }
         operation_ids.insert(op.op.id);
      }
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

graphene::wallet::plain_keys decrypt_keys( const std::string& password, const vector<char>& cipher_keys )
{
   auto pw = fc::sha512::hash( password.c_str(), password.size() );
   vector<char> decrypted = fc::aes_decrypt( pw, cipher_keys );
   return fc::raw::unpack<graphene::wallet::plain_keys>( decrypted );
}

BOOST_FIXTURE_TEST_CASE( get_property,cli_fixture)
{
   BOOST_TEST_MESSAGE("get property start");
   try {
      auto blocks = con.wallet_api_ptr->get_block(2);
      auto global_propertys = con.wallet_api_ptr->get_global_properties();
      auto dynamic_propertys = con.wallet_api_ptr->get_dynamic_global_properties();
      //graphene::object::object_id_type id;
      //con.wallet_api_ptr->get_object("1.3.0");
   }catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( build_transaction,cli_fixture)
{
   BOOST_TEST_MESSAGE("build transaction start");
   try {
      INVOKE(create_new_account);

      auto before_balance = con.wallet_api_ptr->list_account_balances("nicotest");
      signed_transaction transfer_tx = con.wallet_api_ptr->transfer(
         "nicotest", "test1", "10000", "1.3.0", "Here are some CORE token for your new account", false
      ).second;

      BOOST_TEST_MESSAGE("begin_builder_transaction");
      auto handle = con.wallet_api_ptr->begin_builder_transaction();

      for(auto i=0;i<transfer_tx.operations.size();i++)
      {
         auto op = transfer_tx.operations.at(i);
         con.wallet_api_ptr->add_operation_to_builder_transaction(handle,op);
      }

      signed_transaction transfer_tx1 = con.wallet_api_ptr->transfer(
         "nicotest", "test1", "0", "1.3.0", "replace for your transaction", false
      ).second;
    
      for(auto iter = transfer_tx1.operations.begin();iter != transfer_tx1.operations.end();iter++)
      {
         auto op1 = *iter;
         con.wallet_api_ptr->replace_operation_in_builder_transaction(handle,0,op1);
      }

      con.wallet_api_ptr->set_fees_on_builder_transaction(handle);

      auto preview = con.wallet_api_ptr->preview_builder_transaction(handle);

      con.wallet_api_ptr->sign_builder_transaction(handle);

      con.wallet_api_ptr->propose_builder_transaction2(handle,"nicotest");

      auto after_balance = con.wallet_api_ptr->list_account_balances("nicotest");

      //con.wallet_api_ptr->crontab_builder_transaction(0,"nicotest","2019-09-11T04:00:00",10,5,true);

      con.wallet_api_ptr->remove_builder_transaction(0);

   }catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( crontab,cli_fixture)
{
   BOOST_TEST_MESSAGE("crontab start");
   try {
      INVOKE(create_new_account);

      signed_transaction transfer_tx = con.wallet_api_ptr->transfer(
         "nicotest", "test1", "10000", "1.3.0", "Here are some CORE token for your new account", false
      ).second;
     
      BOOST_TEST_MESSAGE("create_crontab");
      con.wallet_api_ptr->create_crontab("nicotest",time_point_sec::maximum(),60,3,transfer_tx.operations,true);//"2019-09-11 12:00:00"

      auto crontabs = con.wallet_api_ptr->list_account_crontab("nicotest",true,true);
      for(auto iter = crontabs.begin() ;iter!= crontabs.end();iter++)
      {
         wdump((*iter));
         //con.wallet_api_ptr->recover_crontab("nathan",iter->id,"2019-09-11 04:45:00",true);
      }
   }catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}