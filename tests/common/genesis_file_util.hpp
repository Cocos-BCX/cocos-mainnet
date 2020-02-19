#pragma once

/*
   @brief forward declaration, using as a hack to generate a genesis.json file for testing
*/
namespace graphene { 
   namespace app { 
      namespace detail {
         graphene::chain::genesis_state_type create_example_genesis();
      } // graphene::app::detail
   } // graphene::app
} // graphene

/*
   @brief create a genesis_json file
   @param directory the directory to place the file "genesis.json"
   @returns the full path to the file
*/
boost::filesystem::path create_genesis_file(fc::temp_directory& directory) 
{
   boost::filesystem::path genesis_path = boost::filesystem::path{directory.path().generic_string()} / "genesis.json";
   fc::path genesis_out = genesis_path;
   graphene::chain::genesis_state_type genesis_state = graphene::app::detail::create_example_genesis();

   /* Work In Progress: Place some accounts in the Genesis file so as to pre-make some accounts to play with
   std::string test_prefix = "test";
   // helper lambda
   auto get_test_key = [&]( std::string prefix, uint32_t i ) -> public_key_type
   {
      return fc::ecc::private_key::regenerate( fc::sha256::hash( test_prefix + prefix + std::to_string(i) ) ).get_public_key();
   };

   // create 2 accounts to use
   for (int i = 1; i <= 2; ++i )
   {
      genesis_state_type::initial_account_type dev_account(
            test_prefix + std::to_string(i),
            get_test_key("owner-", i),
            get_test_key("active-", i),
            false);

      genesis_state.initial_accounts.push_back(dev_account);
      // give her some coin

   }
   */

   fc::json::save_to_file(genesis_state, genesis_out);
   return genesis_path;
}
