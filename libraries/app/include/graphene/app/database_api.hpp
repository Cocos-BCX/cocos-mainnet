/*
 * Copyright (c) 2017 Cryptonomex, Inc., and contributors.
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
#pragma once

#include <graphene/app/full_account.hpp>

#include <graphene/chain/protocol/types.hpp>

#include <graphene/chain/database.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/transaction_object.hpp>
#include <graphene/chain/world_view_object.hpp>
#include <graphene/chain/nh_asset_object.hpp>
#include <graphene/chain/nh_asset_order_object.hpp>
#include <graphene/chain/nh_asset_creator_object.hpp>
#include <graphene/chain/file_object.hpp>
#include <graphene/chain/crontab_object.hpp>

#include <graphene/market_history/market_history_plugin.hpp>

#include <fc/api.hpp>
#include <fc/optional.hpp>
#include <fc/variant_object.hpp>

#include <fc/network/ip.hpp>

#include <boost/container/flat_set.hpp>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace graphene
{
namespace app
{

using namespace graphene::chain;
using namespace graphene::market_history;
using namespace std;

class database_api_impl;

struct order
{
      double price;
      double quote;
      double base;
};

struct order_book
{
      string base;
      string quote;
      vector<order> bids;
      vector<order> asks;
};

struct market_ticker
{
      time_point_sec time;
      string base;
      string quote;
      double latest;
      double lowest_ask;
      double highest_bid;
      double percent_change;
      double base_volume;
      double quote_volume;
};

struct market_volume
{
      time_point_sec time;
      string base;
      string quote;
      double base_volume;
      double quote_volume;
};

struct market_trade
{
      int64_t sequence = 0;
      fc::time_point_sec date;
      double price;
      double amount;
      double value;
      account_id_type side1_account_id = GRAPHENE_NULL_ACCOUNT;
      account_id_type side2_account_id = GRAPHENE_NULL_ACCOUNT;
};

enum class nh_asset_list_type
{
    only_active = 0,        // target with NHA active permission only 
    only_owner = 1,         // target with NHA owner permission only 
    all_active = 2,         // target with NHA active permission
    all_owner = 3,          // target with NHA active permission
    owner_and_active = 4    // target with NHA both active and owner permission
};

/**
 * @brief The database_api class implements the RPC API for the chain database.
 *
 * This API exposes accessors on the database which query state tracked by a blockchain validating node. This API is
 * read-only; all modifications to the database must be performed via transactions. Transactions are broadcast via
 * the @ref network_broadcast_api.
 */
class database_api
{
    public:
      database_api(graphene::chain::database &db);
      ~database_api();

      /////////////
      // Objects //
      /////////////

      /**
       * @brief Get the objects corresponding to the provided IDs
       * @param ids IDs of the objects to retrieve
       * @return The objects retrieved, in the order they are mentioned in ids
       *
       * If any of the provided IDs does not map to an object, a null variant is returned in its position.
       */
      fc::variants get_objects(const vector<object_id_type> &ids) const;

      ///////////////////
      // Subscriptions //
      ///////////////////

      void set_subscribe_callback(std::function<void(const variant &)> cb, bool clear_filter);
      void set_pending_transaction_callback(std::function<void(const variant &)> cb);
      void set_block_applied_callback(std::function<void(const variant &block_id)> cb);
      /**
       * @brief Stop receiving any notifications
       *
       * This unsubscribes from all subscribed markets and objects.
       */
      void cancel_all_subscriptions();

      /////////////////////////////
      // Blocks and transactions //
      /////////////////////////////

      /**
       * @brief Retrieve a block header
       * @param block_num Height of the block whose header should be returned
       * @return header of the referenced block, or null if no matching block was found
       */
      optional<block_header> get_block_header(uint32_t block_num) const;

      /**
      * @brief Retrieve multiple block header by block numbers
      * @param block_num vector containing heights of the block whose header should be returned
      * @return array of headers of the referenced blocks, or null if no matching block was found
      */
      map<uint32_t, optional<block_header>> get_block_header_batch(const vector<uint32_t> block_nums) const;

      /**
       * @brief Retrieve a full, signed block
       * @param block_num Height of the block to be returned
       * @return the referenced block, or null if no matching block was found
       */
      optional<signed_block> get_block(uint32_t block_num) const;

      /**
       * @brief used to fetch an individual transaction.
       */
      processed_transaction get_transaction(uint32_t block_num, uint32_t trx_in_block) const;

      /**
       * If the transaction has not expired, this method will return the transaction for the given ID or
       * it will return NULL if it is not known.  Just because it is not known does not mean it wasn't
       * included in the blockchain.
       */
      optional<signed_transaction> get_recent_transaction_by_id(const string &id) const;

      /////////////
      // Globals //
      /////////////

      /**
       * @brief Retrieve the @ref chain_property_object associated with the chain
       */
      chain_property_object get_chain_properties() const;

      /**
       * @brief Retrieve the current @ref global_property_object
       */
      global_property_object get_global_properties() const;

      /**
       * @brief Retrieve compile-time constants
       */
      fc::variant_object get_config() const;

      /**
       * @brief Get the chain ID
       */
      chain_id_type get_chain_id() const;

      /**
       * @brief Retrieve the current @ref dynamic_global_property_object
       */
      dynamic_global_property_object get_dynamic_global_properties() const;

      //////////
      // Keys //
      //////////

      vector<vector<account_id_type>> get_key_references(vector<public_key_type> key) const;

      /**
      * Determine whether a textual representation of a public key
      * (in Base-58 format) is *currently* linked
      * to any *registered* (i.e. non-stealth) account on the blockchain
      * @param public_key Public key
      * @return Whether a public key is known
      */
      bool is_public_key_registered(string public_key) const;

      //////////////
      // Accounts //
      //////////////

      /**
       * @brief Get a list of accounts by ID
       * @param account_ids IDs of the accounts to retrieve
       * @return The accounts corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<account_object>> get_accounts(const vector<account_id_type> &account_ids) const;

      /**
       * @brief Fetch all objects relevant to the specified accounts and subscribe to updates
       * @param callback Function to call with updates
       * @param names_or_ids Each item must be the name or ID of an account to retrieve
       * @return Map of string from @ref names_or_ids to the corresponding account
       *
       * This function fetches all relevant objects for the given accounts, and subscribes to updates to the given
       * accounts. If any of the strings in @ref names_or_ids cannot be tied to an account, that input will be
       * ignored. All other accounts will be retrieved and subscribed.
       *
       */
      std::map<string, full_account> get_full_accounts(const vector<string> &names_or_ids, bool subscribe);

      optional<account_object> get_account_by_name(string name) const;

      /**
       *  @return all accounts that referr to the key or account id in their owner or active authorities.
       */
      vector<account_id_type> get_account_references(account_id_type account_id) const;

      /**
       * @brief Get a list of accounts by name
       * @param account_names Names of the accounts to retrieve
       * @return The accounts holding the provided names
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<account_object>> lookup_account_names(const vector<string> &account_names) const;

      /**
       * @brief Get names and IDs for registered accounts
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return -- must not exceed 1000
       * @return Map of account names to corresponding IDs
       */
      map<string, account_id_type> lookup_accounts(const string &lower_bound_name, uint32_t limit) const;

      //////////////
      // Balances //
      //////////////

      /**
       * @brief Get an account's balances in various assets
       * @param id ID of the account to get balances for
       * @param assets IDs of the assets to get balances of; if empty, get all assets account has a balance in
       * @return Balances of the account
       */
      vector<asset> get_account_balances(account_id_type id, const flat_set<asset_id_type> &assets) const;

      /// Semantically equivalent to @ref get_account_balances, but takes a name instead of an ID.
      vector<asset> get_named_account_balances(const std::string &name, const flat_set<asset_id_type> &assets) const;

      /** @return all unclaimed balance objects for a set of addresses */
      vector<balance_object> get_balance_objects(const vector<address> &addrs) const;

      vector<asset> get_vested_balances(const vector<balance_id_type> &objs) const;

      vector<vesting_balance_object> get_vesting_balances(account_id_type account_id) const;

      /**
       * @brief Get the total number of accounts registered with the blockchain
       */
      uint64_t get_account_count() const;

      ////////////
      // Assets //
      ////////////

      /**
       * @brief Get a list of assets by ID
       * @param asset_ids IDs of the assets to retrieve
       * @return The assets corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<asset_object>> get_assets(const vector<asset_id_type> &asset_ids) const;

      /**
       * @brief Get assets alphabetically by symbol name
       * @param lower_bound_symbol Lower bound of symbol names to retrieve
       * @param limit Maximum number of assets to fetch (must not exceed 100)
       * @return The assets found
       */
      vector<asset_object> list_assets(const string &lower_bound_symbol, uint32_t limit) const;

      /**
       * @brief Get a list of assets by symbol
       * @param asset_symbols Symbols or stringified IDs of the assets to retrieve
       * @return The assets corresponding to the provided symbols or IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<asset_object>> lookup_asset_symbols(const vector<string> &symbols_or_ids) const;

      /////////////////////
      // Markets / feeds //
      /////////////////////

      /**
       * @brief Get limit orders in a given market
       * @param a ID of asset being sold
       * @param b ID of asset being purchased
       * @param limit Maximum number of orders to retrieve
       * @return The limit orders, ordered from least price to greatest
       */
      vector<limit_order_object> get_limit_orders(asset_id_type a, asset_id_type b, uint32_t limit) const;

      /**
       * @brief Get call orders in a given asset
       * @param a ID of asset being called
       * @param limit Maximum number of orders to retrieve
       * @return The call orders, ordered from earliest to be called to latest
       */
      vector<call_order_object> get_call_orders(asset_id_type a, uint32_t limit) const;

      /**
       * @brief Get forced settlement orders in a given asset
       * @param a ID of asset being settled
       * @param limit Maximum number of orders to retrieve
       * @return The settle orders, ordered from earliest settlement date to latest
       */
      vector<force_settlement_object> get_settle_orders(asset_id_type a, uint32_t limit) const;

      /**
       * @brief Get collateral_bid_objects for a given asset
       * @param a ID of asset
       * @param limit Maximum number of objects to retrieve
       * @param start skip that many results
       * @return The settle orders, ordered from earliest settlement date to latest
       */
      vector<collateral_bid_object> get_collateral_bids(const asset_id_type asset, uint32_t limit, uint32_t start) const;

      /**
       *  @return all open margin positions for a given account id.
       */
      vector<call_order_object> get_margin_positions(const account_id_type &id) const;

      /**
       * @brief Request notification when the active orders in the market between two assets changes
       * @param callback Callback method which is called when the market changes
       * @param a First asset ID
       * @param b Second asset ID
       *
       * Callback will be passed a variant containing a vector<pair<operation, operation_result>>. The vector will
       * contain, in order, the operations which changed the market, and their results.
       */
      void subscribe_to_market(std::function<void(const variant &)> callback,
                               asset_id_type a, asset_id_type b);

      /**
       * @brief Unsubscribe from updates to a given market
       * @param a First asset ID
       * @param b Second asset ID
       */
      void unsubscribe_from_market(asset_id_type a, asset_id_type b);

      /**
       * @brief Returns the ticker for the market assetA:assetB
       * @param a String name of the first asset
       * @param b String name of the second asset
       * @return The market ticker for the past 24 hours.
       */
      market_ticker get_ticker(const string &base, const string &quote) const;

      /**
       * @brief Returns the 24 hour volume for the market assetA:assetB
       * @param a String name of the first asset
       * @param b String name of the second asset
       * @return The market volume over the past 24 hours
       */
      market_volume get_24_volume(const string &base, const string &quote) const;

      /**
       * @brief Returns the order book for the market base:quote
       * @param base String name of the first asset
       * @param quote String name of the second asset
       * @param depth of the order book. Up to depth of each asks and bids, capped at 50. Prioritizes most moderate of each
       * @return Order book of the market
       */
      order_book get_order_book(const string &base, const string &quote, unsigned limit = 50) const;

      /**
       * @brief Returns recent trades for the market assetA:assetB, ordered by time, most recent first. The range is [stop, start)
       * Note: Currently, timezone offsets are not supported. The time must be UTC.
       * @param a String name of the first asset
       * @param b String name of the second asset
       * @param stop Stop time as a UNIX timestamp, the earliest trade to retrieve
       * @param limit Number of trasactions to retrieve, capped at 100
       * @param start Start time as a UNIX timestamp, the latest trade to retrieve
       * @return Recent transactions in the market
       */
      vector<market_trade> get_trade_history(const string &base, const string &quote, fc::time_point_sec start, fc::time_point_sec stop, unsigned limit = 100) const;

      /**
       * @brief Returns trades for the market assetA:assetB, ordered by time, most recent first. The range is [stop, start)
       * Note: Currently, timezone offsets are not supported. The time must be UTC.
       * @param a String name of the first asset
       * @param b String name of the second asset
       * @param stop Stop time as a UNIX timestamp, the earliest trade to retrieve
       * @param limit Number of trasactions to retrieve, capped at 100
       * @param start Start sequence as an Integer, the latest trade to retrieve
       * @return Transactions in the market
       */
      vector<market_trade> get_trade_history_by_sequence(const string &base, const string &quote, int64_t start, fc::time_point_sec stop, unsigned limit = 100) const;

      ///////////////
      // Witnesses //
      ///////////////

      /**
       * @brief Get a list of witnesses by ID
       * @param witness_ids IDs of the witnesses to retrieve
       * @return The witnesses corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<witness_object>> get_witnesses(const vector<witness_id_type> &witness_ids) const;

      /**
       * @brief Get the witness owned by a given account
       * @param account The ID of the account whose witness should be retrieved
       * @return The witness object, or null if the account does not have a witness
       */
      fc::optional<witness_object> get_witness_by_account(account_id_type account) const;

      /**
       * @brief Get names and IDs for registered witnesses
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return -- must not exceed 1000
       * @return Map of witness names to corresponding IDs
       */
      map<string, witness_id_type> lookup_witness_accounts(const string &lower_bound_name, uint32_t limit) const;

      /**
       * @brief Get the total number of witnesses registered with the blockchain
       */
      uint64_t get_witness_count() const;

      ///////////////////////
      // Committee members //
      ///////////////////////

      /**
       * @brief Get a list of committee_members by ID
       * @param committee_member_ids IDs of the committee_members to retrieve
       * @return The committee_members corresponding to the provided IDs
       *
       * This function has semantics identical to @ref get_objects
       */
      vector<optional<committee_member_object>> get_committee_members(const vector<committee_member_id_type> &committee_member_ids) const;

      /**
       * @brief Get the committee_member owned by a given account
       * @param account The ID of the account whose committee_member should be retrieved
       * @return The committee_member object, or null if the account does not have a committee_member
       */
      fc::optional<committee_member_object> get_committee_member_by_account(account_id_type account) const;

      /**
       * @brief Get names and IDs for registered committee_members
       * @param lower_bound_name Lower bound of the first name to return
       * @param limit Maximum number of results to return -- must not exceed 1000
       * @return Map of committee_member names to corresponding IDs
       */
      map<string, committee_member_id_type> lookup_committee_member_accounts(const string &lower_bound_name, uint32_t limit) const;

      /**
       * @brief Get the total number of committee registered with the blockchain
      */
      uint64_t get_committee_count() const;

      ///////////////////////
      // Worker proposals  //
      ///////////////////////

      /**
       * @brief Get all workers
       * @return All the workers
       *
      */
      vector<worker_object> get_all_workers() const;

      /**
       * @brief Get the total number of workers registered with the blockchain
      */
      uint64_t get_worker_count() const;

      ///////////
      // Votes //
      ///////////

      /**
       *  @brief Given a set of votes, return the objects they are voting for.
       *
       *  This will be a mixture of committee_member_object, witness_objects, and worker_objects
       *
       *  The results will be in the same order as the votes.  Null will be returned for
       *  any vote ids that are not found.
       */
      vector<variant> lookup_vote_ids(const flat_set<vote_id_type>&votes) const;

      ////////////////////////////
      // Authority / validation //
      ////////////////////////////

      /// @brief Get a hexdump of the serialized binary form of a transaction
      std::string get_transaction_hex(const signed_transaction &trx) const;

      /**
       *  This API will take a partially signed transaction and a set of public keys that the owner has the ability to sign for
       *  and return the minimal subset of public keys that should add signatures to the transaction.
       */
      set<public_key_type> get_required_signatures(const signed_transaction &trx, const flat_set<public_key_type> &available_keys) const;

      /**
       *  This method will return the set of all public keys that could possibly sign for a given transaction.  This call can
       *  be used by wallets to filter their set of public keys to just the relevant subset prior to calling @ref get_required_signatures
       *  to get the minimum subset.
       */
      set<public_key_type> get_potential_signatures(const signed_transaction &trx) const;
      set<address> get_potential_address_signatures(const signed_transaction &trx) const;

      /**
       * @return true of the @ref trx has all of the required signatures, otherwise throws an exception
       */
      bool verify_authority(const signed_transaction &trx) const;

      /**
       * @return true if the signers have enough authority to authorize an account
       */
      bool verify_account_authority(const string &name_or_id, const flat_set<public_key_type> &signers) const;

      /**
       *  Validates a transaction against the current state without broadcasting it on the network.
       */
      processed_transaction validate_transaction(const signed_transaction &trx) const;

      /**
       *  For each operation calculate the required fee in the specified asset type.  If the asset type does
       *  not have a valid core_exchange_rate
       */
      //vector<fc::variant> get_required_fees(const vector<operation> &ops, asset_id_type id) const;

      ///////////////////////////
      // Proposed transactions //
      ///////////////////////////

      /**
       *  @return the set of proposed transactions relevant to the specified account id.
       */
      vector<proposal_object> get_proposed_transactions(account_id_type id) const;

      //////////////////////
      // Blinded balances //
      //////////////////////


      /*additional members declaration START*/
      operation get_prototype_operation_by_idx(uint index);
      fc::variant get_account_contract_data(account_id_type account_id, const contract_id_type contract_id) const;
      lua_map get_contract_public_data(string contract_id_or_name, lua_map filter) const;
      optional<processed_transaction> get_transaction_by_id(const string &id) const;
      vector<contract_id_type> list_account_contracts(const account_id_type& owner)const;
      optional<contract_object> get_contract(string contract_id_or_name);
      optional<transaction_in_block_info> get_transaction_in_block_info(const string &id) const;
      vector<optional<world_view_object>> lookup_world_view(const vector<string> &world_view_name_or_ids) const;
      vector<optional<nh_asset_object>> lookup_nh_asset(const vector<string> &nh_asset_hash_or_ids) const;
      std::pair<vector<nh_asset_object>, uint32_t> list_nh_asset_by_creator(const account_id_type &nh_asset_creator,const string& world_view,
          uint32_t pagesize,uint32_t page);
      std::pair<vector<nh_asset_object>, uint32_t> list_account_nh_asset(const account_id_type &nh_asset_owner,
          const vector<string> &world_view_name_or_ids,uint32_t pagesize,uint32_t page, nh_asset_list_type list_type = nh_asset_list_type::all_owner);
      optional<nh_asset_creator_object> get_nh_creator(const account_id_type &nh_asset_creator);
      std::pair<vector<nh_asset_order_object>, uint32_t> list_nh_asset_order(const string &asset_symbols_or_id, const string &world_view_name_or_id,
          const string &base_describe = "", uint32_t pagesize = 10, uint32_t page = 1, bool is_ascending_order = true);
      std::pair<vector<nh_asset_order_object>, uint32_t> list_new_nh_asset_order(uint32_t limit);
      std::pair<vector<nh_asset_order_object>, uint32_t> list_account_nh_asset_order(const account_id_type &nh_asset_order_owner,
          uint32_t pagesize,uint32_t page);
      optional<file_object> lookup_file(const string &file_name_or_ids) const;
      map<string, file_id_type> list_account_created_file(const account_id_type &file_creater) const;
      asset estimation_gas(const asset& delta_collateral);
      flat_set<public_key_type> get_signature_keys(const signed_transaction &trx);
      // list crontabs that created by the account
      // crontab_creator:crontab creator
      vector<crontab_object> list_account_crontab(const account_id_type &crontab_creator, bool contain_normal = true, bool contain_suspended = true) const;
      vector<asset_restricted_object> list_asset_restricted_objects(const asset_id_type asset_id,restricted_enum restricted_type) const;
      /*additional members declaration START end*/
     
    private:
      std::shared_ptr<database_api_impl> my;
};

} // namespace app
} // namespace graphene
FC_REFLECT(graphene::app::order, (price)(quote)(base));
FC_REFLECT(graphene::app::order_book, (base)(quote)(bids)(asks));
FC_REFLECT(graphene::app::market_ticker,
           (time)(base)(quote)(latest)(lowest_ask)(highest_bid)(percent_change)(base_volume)(quote_volume));
FC_REFLECT(graphene::app::market_volume, (time)(base)(quote)(base_volume)(quote_volume));
FC_REFLECT(graphene::app::market_trade, (sequence)(date)(price)(amount)(value)(side1_account_id)(side2_account_id));
FC_REFLECT_ENUM(graphene::app::nh_asset_list_type, (only_active)(only_owner)(all_active)(all_owner)(owner_and_active));

FC_API(graphene::app::database_api,
       // Objects
       (get_objects)

       // Subscriptions
       (set_subscribe_callback)(set_pending_transaction_callback)(set_block_applied_callback)(cancel_all_subscriptions)

       // Blocks and transactions
       (get_block_header)(get_block_header_batch)(get_block)(get_transaction)(get_recent_transaction_by_id)

       // Globals
       (get_chain_properties)(get_global_properties)(get_config)(get_chain_id)(get_dynamic_global_properties)

       // Keys
       (get_key_references)(is_public_key_registered)(get_signature_keys)

       // Accounts
       (get_accounts)(get_full_accounts)(get_account_by_name)(get_account_references)(lookup_account_names)(lookup_accounts)(get_account_count)

       // Balances
       (get_account_balances)(get_named_account_balances)(get_balance_objects)(get_vested_balances)(get_vesting_balances)(get_prototype_operation_by_idx)
       // Lua contract
       (list_account_contracts)(get_account_contract_data)(get_contract_public_data)(get_contract)(get_transaction_by_id)(get_transaction_in_block_info)
       //nh asset
       (lookup_world_view)(lookup_nh_asset)(list_nh_asset_by_creator)(list_account_nh_asset)(get_nh_creator)(list_nh_asset_order)(list_new_nh_asset_order)(list_account_nh_asset_order)
       //file
       (lookup_file)(list_account_created_file)
       //crontab
       (list_account_crontab)
       // Assets
       (get_assets)(list_assets)(lookup_asset_symbols)(list_asset_restricted_objects)

       // Markets / feeds
       (get_order_book)(estimation_gas)(get_limit_orders)(get_call_orders)(get_settle_orders)(get_margin_positions)(get_collateral_bids)(subscribe_to_market)(unsubscribe_from_market)(get_ticker)(get_24_volume)(get_trade_history)(get_trade_history_by_sequence)

       // Witnesses
       (get_witnesses)(get_witness_by_account)(lookup_witness_accounts)(get_witness_count)

       // Committee members
       (get_committee_members)(get_committee_member_by_account)(lookup_committee_member_accounts)(get_committee_count)

       // workers
       (get_all_workers)

       // Votes
       (lookup_vote_ids)

       // Authority / validation
       (get_transaction_hex)(get_required_signatures)(get_potential_signatures)(get_potential_address_signatures)(verify_authority)(verify_account_authority)(validate_transaction)

       // Proposed transactions
       (get_proposed_transactions))
