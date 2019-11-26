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

#include <graphene/app/api.hpp>
#include <graphene/utilities/key_conversion.hpp>

using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace std;

namespace fc
{
void to_variant(const account_multi_index_type &accts, variant &vo);
void from_variant(const variant &var, account_multi_index_type &vo);
} // namespace fc

namespace graphene
{
namespace wallet
{

typedef uint16_t transaction_handle_type;

/**
 * This class takes a variant and turns it into an object
 * of the given type, with the new operator.
 */

object *create_object(const variant &v);

struct plain_keys
{
    map<public_key_type, string> keys;
    fc::sha512 checksum;
};

struct brain_key_info
{
    string brain_priv_key;
    string wif_priv_key;
    address address_info;
    public_key_type pub_key;
};

struct key_label
{
    string label;
    public_key_type key;
};

struct by_label;
struct by_key;
typedef multi_index_container<
    key_label,
    indexed_by<
        ordered_unique<tag<by_label>, member<key_label, string, &key_label::label>>,
        ordered_unique<tag<by_key>, member<key_label, public_key_type, &key_label::key>>>>
    key_label_index_type;

struct wallet_data
{
    /** Chain ID this wallet is used with */
    chain_id_type chain_id;
    account_multi_index_type my_accounts;
    /// @return IDs of all accounts in @ref my_accounts
    vector<object_id_type> my_account_ids() const
    {
        vector<object_id_type> ids;
        ids.reserve(my_accounts.size());
        std::transform(my_accounts.begin(), my_accounts.end(), std::back_inserter(ids),
                       [](const account_object &ao) { return ao.id; });
        return ids;
    }
    /// Add acct to @ref my_accounts, or update it if it is already in @ref my_accounts
    /// @return true if the account was newly inserted; false if it was only updated
    bool update_account(const account_object &acct)
    {
        auto &idx = my_accounts.get<by_id>();
        auto itr = idx.find(acct.get_id());
        if (itr != idx.end())
        {
            idx.replace(itr, acct);
            return false;
        }
        else
        {
            idx.insert(acct);
            return true;
        }
    }

    /** encrypted keys */
    vector<char> cipher_keys;

    /** map an account to a set of extra keys that have been imported for that account */
    map<account_id_type, set<public_key_type>> extra_keys;

    // map of account_name -> base58_private_key for
    //   incomplete account regs
    map<string, vector<string>> pending_account_registrations;
    map<string, string> pending_witness_registrations;

    key_label_index_type labeled_keys;

    string ws_server = "ws://localhost:8090";
    string ws_user;
    string ws_password;
};



struct approval_delta
{
    vector<string> active_approvals_to_add;
    vector<string> active_approvals_to_remove;
    vector<string> owner_approvals_to_add;
    vector<string> owner_approvals_to_remove;
    vector<string> key_approvals_to_add;
    vector<string> key_approvals_to_remove;
};

struct worker_vote_delta
{
    flat_set<worker_id_type> vote_for;
    flat_set<worker_id_type> vote_against;
    flat_set<worker_id_type> vote_abstain;
};

struct vesting_balance_object_with_info : public vesting_balance_object
{
    vesting_balance_object_with_info(const vesting_balance_object &vbo, fc::time_point_sec now);
    vesting_balance_object_with_info(const vesting_balance_object_with_info &vbo) = default;

    /**
    * How much is allowed to be withdrawn.
    */
    asset allowed_withdraw;

    /**
    * The time at which allowed_withdrawal was calculated.
    */
    fc::time_point_sec allowed_withdraw_time;
};

namespace detail
{
class wallet_api_impl;
}

/***
 * A utility class for performing various state-less actions that are related to wallets
 */
class utility
{
  public:
    /**
       * Derive any number of *possible* owner keys from a given brain key.
       *
       * NOTE: These keys may or may not match with the owner keys of any account.
       * This function is merely intended to assist with account or key recovery.
       *
       * @see suggest_brain_key()
       *
       * @param brain_key    Brain key
       * @param number_of_desired_keys  Number of desired keys
       * @return A list of keys that are deterministically derived from the brainkey
       */
    static vector<brain_key_info> derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys = 1);
};

struct operation_detail
{
    string memo;
    string description;
    operation_history_object op;
};

/**
 * This wallet assumes it is connected to the database server with a high-bandwidth, low-latency connection and
 * performs minimal caching. This API could be provided locally to be used by a web interface.
 */
class wallet_api
{
  public:
    wallet_api(const wallet_data &initial_data, fc::api<login_api> rapi);
    virtual ~wallet_api();

    bool copy_wallet_file(string destination_filename);

    fc::ecc::private_key derive_private_key(const std::string &prefix_string, int sequence_number) const;

    variant info();
    /** Returns info such as client version, git version of graphene/fc, version of boost, openssl.
       * @returns compile time info and client and dependencies versions
       */
    variant_object about() const;
    fc::optional<signed_block> get_block(uint32_t num);
    /** Returns the number of accounts registered on the blockchain
       * @returns the number of registered accounts
       */
    uint64_t get_account_count() const;
    /** Lists all accounts controlled by this wallet.
       * This returns a list of the full account objects for all accounts whose private keys 
       * we possess.
       * @returns a list of account objects
       */
    vector<account_object> list_my_accounts();
    /** Lists all accounts registered in the blockchain.
       * This returns a list of all account names and their account ids, sorted by account name.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all accounts,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last account name returned as the \c lowerbound for the next \c list_accounts() call.
       *
       * @param lowerbound the name of the first account to return.  If the named account does not exist, 
       *                   the list will start at the account that comes after \c lowerbound
       * @param limit the maximum number of accounts to return (max: 1000)
       * @returns a list of accounts mapping account names to account ids
       */
    map<string, account_id_type> list_accounts(const string &lowerbound, uint32_t limit);
    /** List the balances of an account.
       * Each account can have multiple balances, one for each type of asset owned by that 
       * account.  The returned list will only contain assets for which the account has a
       * nonzero balance
       * @param id the name or id of the account whose balances you want
       * @returns a list of the given account's balances
       */
    vector<asset> list_account_balances(const string &id);
    /** Lists all assets registered on the blockchain.
       * 
       * To list all assets, pass the empty string \c "" for the lowerbound to start
       * at the beginning of the list, and iterate as necessary.
       *
       * @param lowerbound  the symbol of the first asset to include in the list.
       * @param limit the maximum number of assets to return (max: 100)
       * @returns the list of asset objects, ordered by symbol
       */
    vector<asset_object> list_assets(const string &lowerbound, uint32_t limit) const;

    /** Returns the most recent operations on the named account.
       *
       * This returns a list of operation history objects, which describe activity on the account.
       *
       * @param name the name or id of the account
       * @param limit the number of entries to return (starting from the most recent)
       * @returns a list of \c operation_history_objects
       */
    vector<operation_detail> get_account_history(string name, int limit) const;

    /** Returns the relative operations on the named account from start number.
       *
       * @param name the name or id of the account
       * @param stop Sequence number of earliest operation.
       * @param limit the number of entries to return
       * @param start  the sequence number where to start looping back throw the history
       * @returns a list of \c operation_history_objects
       */
    vector<operation_detail> get_relative_account_history(string name, uint32_t stop, int limit, uint32_t start) const;

    vector<bucket_object> get_market_history(string symbol, string symbol2, uint32_t bucket, fc::time_point_sec start, fc::time_point_sec end) const;
    vector<limit_order_object> get_limit_orders(string a, string b, uint32_t limit) const;
    vector<call_order_object> get_call_orders(string a, uint32_t limit) const;
    vector<force_settlement_object> get_settle_orders(string a, uint32_t limit) const;

    /** Returns the collateral_bid object for the given MPA
       *
       * @param asset the name or id of the asset
       * @param limit the number of entries to return
       * @param start the sequence number where to start looping back throw the history
       * @returns a list of \c collateral_bid_objects
       */
    vector<collateral_bid_object> get_collateral_bids(string asset, uint32_t limit = 100, uint32_t start = 0) const;

    /** Returns the block chain's slowly-changing settings.
       * This object contains all of the properties of the blockchain that are fixed
       * or that change only once per maintenance interval (daily) such as the
       * current list of witnesses, committee_members, block interval, etc.
       * @see \c get_dynamic_global_properties() for frequently changing properties
       * @returns the global properties
       */
    global_property_object get_global_properties() const;

    /** Returns the block chain's rapidly-changing properties.
       * The returned object contains information that changes every block interval
       * such as the head block number, the next witness, etc.
       * @see \c get_global_properties() for less-frequently changing properties
       * @returns the dynamic global properties
       */
    dynamic_global_property_object get_dynamic_global_properties() const;

    /** Returns information about the given account.
       *
       * @param account_name_or_id the name or id of the account to provide information about
       * @returns the public account data stored in the blockchain
       */
    account_object get_account(string account_name_or_id) const;

    /** Returns information about the given asset.
       * @param asset_name_or_id the symbol or id of the asset in question
       * @returns the information about the asset stored in the block chain
       */
    asset_object get_asset(string asset_name_or_id) const;

    /** Returns the BitAsset-specific data for a given asset.
       * Market-issued assets's behavior are determined both by their "BitAsset Data" and
       * their basic asset data, as returned by \c get_asset().
       * @param asset_name_or_id the symbol or id of the BitAsset in question
       * @returns the BitAsset-specific data for this asset
       */
    asset_bitasset_data_object get_bitasset_data(string asset_name_or_id) const;

    /** Lookup the id of a named account.
       * @param account_name_or_id the name of the account to look up
       * @returns the id of the named account
       */
    account_id_type get_account_id(string account_name_or_id) const;

    /**
       * Lookup the id of a named asset.
       * @param asset_name_or_id the symbol of an asset to look up
       * @returns the id of the given asset
       */
    asset_id_type get_asset_id(string asset_name_or_id) const;

    /**
       * Returns the blockchain object corresponding to the given id.
       *
       * This generic function can be used to retrieve any object from the blockchain
       * that is assigned an ID.  Certain types of objects have specialized convenience 
       * functions to return their objects -- e.g., assets have \c get_asset(), accounts 
       * have \c get_account(), but this function will work for any object.
       *
       * @param id the id of the object to return
       * @returns the requested object
       */
    variant get_object(object_id_type id) const;

    /** Returns the current wallet filename.  
       *
       * This is the filename that will be used when automatically saving the wallet.
       *
       * @see set_wallet_filename()
       * @return the wallet filename
       */
    string get_wallet_filename() const;

    /**
       * Get the WIF private key corresponding to a public key.  The
       * private key must already be in the wallet.
       */
    string get_private_key(public_key_type pubkey) const;

    /**
       * @ingroup Transaction Builder API
       */
    transaction_handle_type begin_builder_transaction();
    /**
       * @ingroup Transaction Builder API
       */
    void add_operation_to_builder_transaction(transaction_handle_type transaction_handle, const operation &op);
    /**
       * @ingroup Transaction Builder API
       */
    void replace_operation_in_builder_transaction(transaction_handle_type handle,
                                                  unsigned operation_index,
                                                  const operation &new_op);
    /**
       * @ingroup Transaction Builder API
       */
    transaction preview_builder_transaction(transaction_handle_type handle);
    /**
       * @ingroup Transaction Builder API
       */
    pair<tx_hash_type, signed_transaction> sign_builder_transaction(transaction_handle_type transaction_handle, bool broadcast = true);
    /**
       * @ingroup Transaction Builder API
       */

    pair<tx_hash_type, signed_transaction> propose_builder_transaction(
        transaction_handle_type handle,
        string account_name_or_id,
        time_point_sec expiration = time_point::now() + fc::minutes(1),
        uint32_t review_period_seconds = 0,
        bool broadcast = true);

    /**
       * @ingroup Transaction Builder API
       */
    void remove_builder_transaction(transaction_handle_type handle);

    /** Checks whether the wallet has just been created and has not yet had a password set.
       *
       * Calling \c set_password will transition the wallet to the locked state.
       * @return true if the wallet is new
       * @ingroup Wallet Management
       */
    bool is_new() const;

    /** Checks whether the wallet is locked (is unable to use its private keys).  
       *
       * This state can be changed by calling \c lock() or \c unlock().
       * @return true if the wallet is locked
       * @ingroup Wallet Management
       */
    bool is_locked() const;

    /** Locks the wallet immediately.
       * @ingroup Wallet Management
       */
    void lock();

    /** Unlocks the wallet.  
       *
       * The wallet remain unlocked until the \c lock is called
       * or the program exits.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
    void unlock(string password);

    /** Sets a new password on the wallet.
       *
       * The wallet must be either 'new' or 'unlocked' to
       * execute this command.
       * @ingroup Wallet Management
       */
    void set_password(string password);

    /** Dumps all private keys owned by the wallet.
       *
       * The keys are printed in WIF format.  You can import these keys into another wallet
       * using \c import_key()
       * @returns a map containing the private keys, indexed by their public key 
       */
    map<public_key_type, string> dump_private_keys();

    /** Returns a list of all commands supported by the wallet API.
       *
       * This lists each command, along with its arguments and return types.
       * For more detailed help on a single command, use \c get_help()
       *
       * @returns a multi-line string suitable for displaying on a terminal
       */
    string help() const;

    /** Returns detailed help on a single API command.
       * @param method the name of the API command you want help with
       * @returns a multi-line string suitable for displaying on a terminal
       */
    string gethelp(const string &method) const;

    /** Loads a specified Graphene wallet.
       *
       * The current wallet is closed before the new wallet is loaded.
       *
       * @warning This does not change the filename that will be used for future
       * wallet writes, so this may cause you to overwrite your original
       * wallet unless you also call \c set_wallet_filename()
       *
       * @param wallet_filename the filename of the wallet JSON file to load.
       *                        If \c wallet_filename is empty, it reloads the
       *                        existing wallet file
       * @returns true if the specified wallet is loaded
       */
    bool load_wallet_file(string wallet_filename = "");

    /** Saves the current wallet to the given filename.
       * 
       * @warning This does not change the wallet filename that will be used for future
       * writes, so think of this function as 'Save a Copy As...' instead of
       * 'Save As...'.  Use \c set_wallet_filename() to make the filename
       * persist.
       * @param wallet_filename the filename of the new wallet JSON file to create
       *                        or overwrite.  If \c wallet_filename is empty,
       *                        save to the current filename.
       */
    void save_wallet_file(string wallet_filename = "");

    /** Sets the wallet filename used for future writes.
       *
       * This does not trigger a save, it only changes the default filename
       * that will be used the next time a save is triggered.
       *
       * @param wallet_filename the new filename to use for future saves
       */
    void set_wallet_filename(string wallet_filename);

    /** Suggests a safe brain key to use for creating your account.
       * \c create_account_with_brain_key() requires you to specify a 'brain key',
       * a long passphrase that provides enough entropy to generate cyrptographic
       * keys.  This function will suggest a suitably random string that should
       * be easy to write down (and, with effort, memorize).
       * @returns a suggested brain_key
       */
    brain_key_info suggest_brain_key() const;

    /**
      * Derive any number of *possible* owner keys from a given brain key.
      *
      * NOTE: These keys may or may not match with the owner keys of any account.
      * This function is merely intended to assist with account or key recovery.
      *
      * @see suggest_brain_key()
      *
      * @param brain_key    Brain key
      * @param number_of_desired_keys  Number of desired keys
      * @return A list of keys that are deterministically derived from the brainkey
      */
    vector<brain_key_info> derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys = 1) const;

    /**
      * Determine whether a textual representation of a public key
      * (in Base-58 format) is *currently* linked
      * to any *registered* (i.e. non-stealth) account on the blockchain
      * @param public_key Public key
      * @return Whether a public key is known
      */
    bool is_public_key_registered(string public_key) const;

    /** Converts a signed_transaction in JSON form to its binary representation.
       *
       * TODO: I don't see a broadcast_transaction() function, do we need one?
       *
       * @param tx the transaction to serialize
       * @returns the binary form of the transaction.  It will not be hex encoded, 
       *          this returns a raw string that may have null characters embedded 
       *          in it
       */
    string serialize_transaction(signed_transaction tx) const;

    /** Imports the private key for an existing account.
       *
       * The private key must match either an owner key or an active key for the
       * named account.  
       *
       * @see dump_private_keys()
       *
       * @param account_name_or_id the account owning the key
       * @param wif_key the private key in WIF format
       * @returns true if the key was imported
       */
    bool import_key(string account_name_or_id, string wif_key);


    /**
       * This call will construct transaction(s) that will claim all balances controled
       * by wif_keys and deposit them into the given account.
       */
    vector<signed_transaction> import_balance(string account_name_or_id, const vector<string> &wif_keys, bool broadcast);

    /** Transforms a brain key to reduce the chance of errors when re-entering the key from memory.
       *
       * This takes a user-supplied brain key and normalizes it into the form used
       * for generating private keys.  In particular, this upper-cases all ASCII characters
       * and collapses multiple spaces into one.
       * @param s the brain key as supplied by the user
       * @returns the brain key in its normalized form
       */
    string normalize_brain_key(string s) const;

    /** Registers a third party's account on the blockckain.
       *
       * This function is used to register an account for which you do not own the private keys.
       * When acting as a registrar, an end user will generate their own private keys and send
       * you the public keys.  The registrar will use this function to register the account
       * on behalf of the end user.
       *
       * @see create_account_with_brain_key()
       *
       * @param name the name of the account, must be unique on the blockchain.  Shorter names
       *             are more expensive to register; the rules are still in flux, but in general
       *             names of more than 8 characters with at least one digit will be cheap.
       * @param owner the owner key for the new account
       * @param active the active key for the new account
       * @param registrar_account the account which will pay the fee to register the user
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering the account
       */
    pair<tx_hash_type, signed_transaction> register_account(string name,
                                                            public_key_type owner,
                                                            public_key_type active,
                                                            string registrar_account,
                                                            bool broadcast = false);

    /**
       *  Upgrades an account to prime status.
       *  This makes the account holder a 'lifetime member'.
       *
       *  @todo there is no option for annual membership
       *  @param name the name or id of the account to upgrade
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction upgrading the account
       */
    pair<tx_hash_type, signed_transaction> upgrade_account(string name, bool broadcast);

    /** Creates a new account and registers it on the blockchain.
       *
       *
       * @see suggest_brain_key()
       * @see register_account()
       *
       * @param brain_key the brain key used for generating the account's private keys
       * @param account_name the name of the account, must be unique on the blockchain.  Shorter names
       *                     are more expensive to register; the rules are still in flux, but in general
       *                     names of more than 8 characters with at least one digit will be cheap.
       * @param registrar_account the account which will pay the fee to register the user
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering the account
       */
    pair<tx_hash_type, signed_transaction> create_account_with_brain_key(string brain_key,
                                                                         string account_name,
                                                                         string registrar_account,
                                                                         bool broadcast = false);

    /** Transfer an amount from one account to another.
       * @param from the name or id of the account sending the funds
       * @param to the name or id of the account receiving the funds
       * @param amount the amount to send (in nominal units -- to send half of a BTS, specify 0.5)
       * @param asset_symbol the symbol or id of the asset to send
       * @param memo a memo to attach to the transaction.  The memo will be encrypted in the 
       *             transaction and readable for the receiver.  There is no length limit
       *             other than the limit imposed by maximum transaction size, but transaction
       *             increase with transaction size
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction transferring funds
       */
    pair<tx_hash_type, signed_transaction> transfer(string from,
                                                    string to,
                                                    string amount,
                                                    string asset_symbol,
                                                     pair<string, bool> memo,
                                                    bool broadcast = false);

    /**
       *  This method works just like transfer, except it always broadcasts and
       *  returns the transaction ID along with the signed transaction.
       */
    pair<tx_hash_type, signed_transaction> transfer2(string from,
                                                     string to,
                                                     string amount,
                                                     string asset_symbol,
                                                      pair<string, bool> memo)
    {
        auto trx = transfer(from, to, amount, asset_symbol, memo, true);
        return trx; //nico add:: 包装tx 与tx_hash
    }

    /**
       *  This method is used to convert a JSON transaction to its transactin ID.
       */
    transaction_id_type get_transaction_id(const signed_transaction &trx) const { return trx.id(); }

    /** Sign a memo message.
       *
       * @param from the name or id of signing account; or a public key.
       * @param to the name or id of receiving account; or a public key.
       * @param memo text to sign.
       */
    memo_data sign_memo(string from, string to, string memo);

    /** Read a memo.
       *
       * @param memo JSON-enconded memo.
       * @returns string with decrypted message..
       */
    string read_memo(const memo_data &memo);

    /** These methods are used for stealth transfers */
    ///@{
    /**
       *  This method can be used to set the label for a public key
       *
       *  @note No two keys can have the same label.
       *
       *  @return true if the label was set, otherwise false
       */
    bool set_key_label(public_key_type, string label);
    string get_key_label(public_key_type) const;

    /** @return the public key associated with the given label */
    public_key_type get_public_key(string label) const;
    ///@}


    /** Place a limit order attempting to sell one asset for another.
       *
       * Buying and selling are the same operation on Graphene; if you want to buy BTS 
       * with USD, you should sell USD for BTS.
       *
       * The blockchain will attempt to sell the \c symbol_to_sell for as
       * much \c symbol_to_receive as possible, as long as the price is at 
       * least \c min_to_receive / \c amount_to_sell.   
       *
       * In addition to the transaction fees, market fees will apply as specified 
       * by the issuer of both the selling asset and the receiving asset as
       * a percentage of the amount exchanged.
       *
       * If either the selling asset or the receiving asset is whitelist
       * restricted, the order will only be created if the seller is on
       * the whitelist of the restricted asset type.
       *
       * Market orders are matched in the order they are included
       * in the block chain.
       *
       * @todo Allow order expiration to be set here.  Document default/max expiration time
       *
       * @param seller_account the account providing the asset being sold, and which will 
       *                       receive the proceeds of the sale.
       * @param amount_to_sell the amount of the asset being sold to sell (in nominal units)
       * @param symbol_to_sell the name or id of the asset to sell
       * @param min_to_receive the minimum amount you are willing to receive in return for
       *                       selling the entire amount_to_sell
       * @param symbol_to_receive the name or id of the asset you wish to receive
       * @param timeout_sec if the order does not fill immediately, this is the length of 
       *                    time the order will remain on the order books before it is 
       *                    cancelled and the un-spent funds are returned to the seller's 
       *                    account
       * @param fill_or_kill if true, the order will only be included in the blockchain
       *                     if it is filled immediately; if false, an open order will be
       *                     left on the books to fill any amount that cannot be filled
       *                     immediately.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction selling the funds
       */
    pair<tx_hash_type, signed_transaction> sell_asset(string seller_account,
                                                      string amount_to_sell,
                                                      string symbol_to_sell,
                                                      string min_to_receive,
                                                      string symbol_to_receive,
                                                      uint32_t timeout_sec = 0,
                                                      bool fill_or_kill = false,
                                                      bool broadcast = false);

    /** Place a limit order attempting to sell one asset for another.
       * 
       * This API call abstracts away some of the details of the sell_asset call to be more
       * user friendly. All orders placed with sell never timeout and will not be killed if they
       * cannot be filled immediately. If you wish for one of these parameters to be different, 
       * then sell_asset should be used instead.
       *
       * @param seller_account the account providing the asset being sold, and which will
       *                       receive the processed of the sale.
       * @param base The name or id of the asset to sell.
       * @param quote The name or id of the asset to recieve.
       * @param rate The rate in base:quote at which you want to sell.
       * @param amount The amount of base you want to sell.
       * @param broadcast true to broadcast the transaction on the network.
       * @returns The signed transaction selling the funds.                 
       */
    pair<tx_hash_type, signed_transaction> sell(string seller_account,
                                                string base,
                                                string quote,
                                                double rate,
                                                double amount,
                                                bool broadcast);

    /** Place a limit order attempting to buy one asset with another.
       *
       * This API call abstracts away some of the details of the sell_asset call to be more
       * user friendly. All orders placed with buy never timeout and will not be killed if they
       * cannot be filled immediately. If you wish for one of these parameters to be different,
       * then sell_asset should be used instead.
       *
       * @param buyer_account The account buying the asset for another asset.
       * @param base The name or id of the asset to buy.
       * @param quote The name or id of the assest being offered as payment.
       * @param rate The rate in base:quote at which you want to buy.
       * @param amount the amount of base you want to buy.
       * @param broadcast true to broadcast the transaction on the network.
       * @param The signed transaction selling the funds.
       */
    pair<tx_hash_type, signed_transaction> buy(string buyer_account,
                                               string base,
                                               string quote,
                                               double rate,
                                               double amount,
                                               bool broadcast);

    /** Borrow an asset or update the debt/collateral ratio for the loan.
       *
       * This is the first step in shorting an asset.  Call \c sell_asset() to complete the short.
       *
       * @param borrower_name the name or id of the account associated with the transaction.
       * @param amount_to_borrow the amount of the asset being borrowed.  Make this value
       *                         negative to pay back debt.
       * @param asset_symbol the symbol or id of the asset being borrowed.
       * @param amount_of_collateral the amount of the backing asset to add to your collateral
       *        position.  Make this negative to claim back some of your collateral.
       *        The backing asset is defined in the \c bitasset_options for the asset being borrowed.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction borrowing the asset
       */
    pair<tx_hash_type, signed_transaction> borrow_asset(string borrower_name, string amount_to_borrow, string asset_symbol,
                                                        string amount_of_collateral, bool broadcast = false);

    /** Cancel an existing order
       *
       * @param order_id the id of order to be cancelled
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction canceling the order
       */
    pair<tx_hash_type, signed_transaction> cancel_order(object_id_type order_id, bool broadcast = false);

    /** Creates a new user-issued or market-issued asset.
       *
       * Many options can be changed later using \c update_asset()
       *
       * Right now this function is difficult to use because you must provide raw JSON data
       * structures for the options objects, and those include prices and asset ids.
       *
       * @param issuer the name or id of the account who will pay the fee and become the 
       *               issuer of the new asset.  This can be updated later
       * @param symbol the ticker symbol of the new asset
       * @param precision the number of digits of precision to the right of the decimal point,
       *                  must be less than or equal to 12
       * @param common asset options required for all new assets.
       *               Note that core_exchange_rate technically needs to store the asset ID of 
       *               this new asset. Since this ID is not known at the time this operation is 
       *               created, create this price as though the new asset has instance ID 1, and
       *               the chain will overwrite it with the new asset's ID.
       * @param bitasset_opts options specific to BitAssets.  This may be null unless the
       *               \c market_issued flag is set in common.flags
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction creating a new asset
       */
    pair<tx_hash_type, signed_transaction> create_asset(string issuer,
                                                        string symbol,
                                                        uint8_t precision,
                                                        asset_options common,
                                                        fc::optional<bitasset_options> bitasset_opts,
                                                        bool broadcast = false);

    /** Issue new shares of an asset.
       *
       * @param to_account the name or id of the account to receive the new shares
       * @param amount the amount to issue, in nominal units
       * @param symbol the ticker symbol of the asset to issue
       * @param memo a memo to include in the transaction, readable by the recipient
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction issuing the new shares
       */
    pair<tx_hash_type, signed_transaction> issue_asset(string to_account, string amount,
                                                       string symbol,
                                                       pair<string, bool> memo,
                                                       bool broadcast = false);

    /** Update the core options on an asset.
       * There are a number of options which all assets in the network use. These options are 
       * enumerated in the asset_object::asset_options struct. This command is used to update 
       * these options for an existing asset.
       *
       * @note This operation cannot be used to update BitAsset-specific options. For these options,
       * \c update_bitasset() instead.
       *
       * @param symbol the name or id of the asset to update
       * @param new_issuer if changing the asset's issuer, the name or id of the new issuer.
       *                   null if you wish to remain the issuer of the asset
       * @param new_options the new asset_options object, which will entirely replace the existing
       *                    options.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the asset
       */
    pair<tx_hash_type, signed_transaction> update_asset(string symbol,
                                                        fc::optional<string> new_issuer,
                                                        asset_options new_options,
                                                        bool broadcast = false);

    /** Update the options specific to a BitAsset.
       *
       * BitAssets have some options which are not relevant to other asset types. This operation is used to update those
       * options an an existing BitAsset.
       *
       * @see update_asset()
       *
       * @param symbol the name or id of the asset to update, which must be a market-issued asset
       * @param new_options the new bitasset_options object, which will entirely replace the existing
       *                    options.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the bitasset
       */
    pair<tx_hash_type, signed_transaction> update_bitasset(string symbol,
                                                           bitasset_options new_options,
                                                           bool broadcast = false);

    /** Update the set of feed-producing accounts for a BitAsset.
       *
       * BitAssets have price feeds selected by taking the median values of recommendations from a set of feed producers.
       * This command is used to specify which accounts may produce feeds for a given BitAsset.
       * @param symbol the name or id of the asset to update
       * @param new_feed_producers a list of account names or ids which are authorized to produce feeds for the asset.
       *                           this list will completely replace the existing list
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the bitasset's feed producers
       */
    pair<tx_hash_type, signed_transaction> update_asset_feed_producers(string symbol,
                                                                       flat_set<string> new_feed_producers,
                                                                       bool broadcast = false);

    /** Publishes a price feed for the named asset.
       *
       * Price feed providers use this command to publish their price feeds for market-issued assets. A price feed is
       * used to tune the market for a particular market-issued asset. For each value in the feed, the median across all
       * committee_member feeds for that asset is calculated and the market for the asset is configured with the median of that
       * value.
       *
       * The feed object in this command contains three prices: a call price limit, a short price limit, and a settlement price.
       * The call limit price is structured as (collateral asset) / (debt asset) and the short limit price is structured
       * as (asset for sale) / (collateral asset). Note that the asset IDs are opposite to eachother, so if we're
       * publishing a feed for USD, the call limit price will be CORE/USD and the short limit price will be USD/CORE. The
       * settlement price may be flipped either direction, as long as it is a ratio between the market-issued asset and
       * its collateral.
       *
       * @param publishing_account the account publishing the price feed
       * @param symbol the name or id of the asset whose feed we're publishing
       * @param feed the price_feed object containing the three prices making up the feed
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the price feed for the given asset
       */
    pair<tx_hash_type, signed_transaction> publish_asset_feed(string publishing_account,
                                                              string symbol,
                                                              price_feed feed,
                                                              bool broadcast = false);

    /** Burns the given user-issued asset.
       *
       * This command burns the user-issued asset to reduce the amount in circulation.
       * @note you cannot burn market-issued assets.
       * @param from the account containing the asset you wish to burn
       * @param amount the amount to burn, in nominal units
       * @param symbol the name or id of the asset to burn
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction burning the asset
       */
    pair<tx_hash_type, signed_transaction> reserve_asset(string from,
                                                         string amount,
                                                         string symbol,
                                                         bool broadcast = false);

    /** Forces a global settling of the given asset (black swan or prediction markets).
       *
       * In order to use this operation, asset_to_settle must have the global_settle flag set
       *
       * When this operation is executed all balances are converted into the backing asset at the
       * settle_price and all open margin positions are called at the settle price.  If this asset is
       * used as backing for other bitassets, those bitassets will be force settled at their current
       * feed price.
       *
       * @note this operation is used only by the asset issuer, \c settle_asset() may be used by 
       *       any user owning the asset
       *
       * @param symbol the name or id of the asset to force settlement on
       * @param settle_price the price at which to settle
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction settling the named asset
       */
    pair<tx_hash_type, signed_transaction> global_settle_asset(string symbol,
                                                               price settle_price,
                                                               bool broadcast = false);

    /** Schedules a market-issued asset for automatic settlement.
       *
       * Holders of market-issued assests may request a forced settlement for some amount of their asset. This means that
       * the specified sum will be locked by the chain and held for the settlement period, after which time the chain will
       * choose a margin posision holder and buy the settled asset using the margin's collateral. The price of this sale
       * will be based on the feed price for the market-issued asset being settled. The exact settlement price will be the
       * feed price at the time of settlement with an offset in favor of the margin position, where the offset is a
       * blockchain parameter set in the global_property_object.
       *
       * @param account_to_settle the name or id of the account owning the asset
       * @param amount_to_settle the amount of the named asset to schedule for settlement
       * @param symbol the name or id of the asset to settlement on
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction settling the named asset
       */
    pair<tx_hash_type, signed_transaction> settle_asset(string account_to_settle,
                                                        string amount_to_settle,
                                                        string symbol,
                                                        bool broadcast = false);

    /** Creates or updates a bid on an MPA after global settlement.
       *
       * In order to revive a market-pegged asset after global settlement (aka
       * black swan), investors can bid collateral in order to take over part of
       * the debt and the settlement fund, see BSIP-0018. Updating an existing
       * bid to cover 0 debt will delete the bid.
       *
       * @param bidder_name the name or id of the account making the bid
       * @param debt_amount the amount of debt of the named asset to bid for
       * @param debt_symbol the name or id of the MPA to bid for
       * @param additional_collateral the amount of additional collateral to bid
       *        for taking over debt_amount. The asset type of this amount is
       *        determined automatically from debt_symbol.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction creating/updating the bid
       */
    pair<tx_hash_type, signed_transaction> bid_collateral(string bidder_name, string debt_amount, string debt_symbol,
                                                          string additional_collateral, bool broadcast = false);

    /** Whitelist and blacklist accounts, primarily for transacting in whitelisted assets.
       *
       * Accounts can freely specify opinions about other accounts, in the form of either whitelisting or blacklisting
       * them. This information is used in chain validation only to determine whether an account is authorized to transact
       * in an asset type which enforces a whitelist, but third parties can use this information for other uses as well,
       * as long as it does not conflict with the use of whitelisted assets.
       *
       * An asset which enforces a whitelist specifies a list of accounts to maintain its whitelist, and a list of
       * accounts to maintain its blacklist. In order for a given account A to hold and transact in a whitelisted asset S,
       * A must be whitelisted by at least one of S's whitelist_authorities and blacklisted by none of S's
       * blacklist_authorities. If A receives a balance of S, and is later removed from the whitelist(s) which allowed it
       * to hold S, or added to any blacklist S specifies as authoritative, A's balance of S will be frozen until A's
       * authorization is reinstated.
       *
       * @param authorizing_account the account who is doing the whitelisting
       * @param account_to_list the account being whitelisted
       * @param new_listing_status the new whitelisting status
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction changing the whitelisting status
       */
   /*
    pair<tx_hash_type, signed_transaction> whitelist_account(string authorizing_account,
                                                             string account_to_list,
                                                             account_whitelist_operation::account_listing new_listing_status,
                                                             bool broadcast = false);
   */
    /** Creates a committee_member object owned by the given account.
       *
       * An account can have at most one committee_member object.
       *
       * @param owner_account the name or id of the account which is creating the committee_member
       * @param url a URL to include in the committee_member record in the blockchain.  Clients may
       *            display this when showing a list of committee_members.  May be blank.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a committee_member
       */
    pair<tx_hash_type, signed_transaction> create_committee_member(string owner_account,
                                                                   string url,
                                                                   bool broadcast = false);
     pair<tx_hash_type, signed_transaction> update_committee_member(string committee_name,
                                                          string url,
                                                          bool work_status,
                                                          bool broadcast = false);

    /** Lists all witnesses registered in the blockchain.
       * This returns a list of all account names that own witnesses, and the associated witness id,
       * sorted by name.  This lists witnesses whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all witnesss,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last witness name returned as the \c lowerbound for the next \c list_witnesss() call.
       *
       * @param lowerbound the name of the first witness to return.  If the named witness does not exist, 
       *                   the list will start at the witness that comes after \c lowerbound
       * @param limit the maximum number of witnesss to return (max: 1000)
       * @returns a list of witnesss mapping witness names to witness ids
       */
    map<string, witness_id_type> list_witnesses(const string &lowerbound, uint32_t limit);

    /** Lists all committee_members registered in the blockchain.
       * This returns a list of all account names that own committee_members, and the associated committee_member id,
       * sorted by name.  This lists committee_members whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all committee_members,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last committee_member name returned as the \c lowerbound for the next \c list_committee_members() call.
       *
       * @param lowerbound the name of the first committee_member to return.  If the named committee_member does not exist, 
       *                   the list will start at the committee_member that comes after \c lowerbound
       * @param limit the maximum number of committee_members to return (max: 1000)
       * @returns a list of committee_members mapping committee_member names to committee_member ids
       */
    map<string, committee_member_id_type> list_committee_members(const string &lowerbound, uint32_t limit);

    /** Returns information about the given witness.
       * @param owner_account the name or id of the witness account owner, or the id of the witness
       * @returns the information about the witness stored in the block chain
       */
    witness_object get_witness(string owner_account);

    /** Returns information about the given committee_member.
       * @param owner_account the name or id of the committee_member account owner, or the id of the committee_member
       * @returns the information about the committee_member stored in the block chain
       */
    committee_member_object get_committee_member(string owner_account);

    /** Creates a witness object owned by the given account.
       *
       * An account can have at most one witness object.
       *
       * @param owner_account the name or id of the account which is creating the witness
       * @param url a URL to include in the witness record in the blockchain.  Clients may
       *            display this when showing a list of witnesses.  May be blank.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a witness
       */
    pair<tx_hash_type, signed_transaction> create_witness(string owner_account,
                                                          string url,
                                                          bool broadcast = false);

    /**
       * Update a witness object owned by the given account.
       *
       * @param witness_name The name of the witness's owner account.  Also accepts the ID of the owner account or the ID of the witness.
       * @param url Same as for create_witness.  The empty string makes it remain the same.
       * @param block_signing_key The new block signing public key.  The empty string makes it remain the same.
       * @param broadcast true if you wish to broadcast the transaction.
       */
    pair<tx_hash_type, signed_transaction> update_witness(string witness_name,
                                                          string url,
                                                          string block_signing_key,
                                                          bool work_status,
                                                          bool broadcast = false);



    /**
       * Get information about a vesting balance object.
       *
       * @param account_name An account name, account ID, or vesting balance object ID.
       */
    vector<vesting_balance_object_with_info> get_vesting_balances(string account_name);

    /**
       * Withdraw a vesting balance.
       *
       * @param witness_name The account name of the witness, also accepts account ID or vesting balance ID type.
       * @param amount The amount to withdraw.
       * @param asset_symbol The symbol of the asset to withdraw.
       * @param broadcast true if you wish to broadcast the transaction
       */
    pair<tx_hash_type, signed_transaction> withdraw_vesting(
        string witness_name,
        string amount,
        string asset_symbol,
        bool broadcast = false);

    /** Vote for a given committee_member.
       *
       * An account can publish a list of all committee_memberes they approve of.  This 
       * command allows you to add or remove committee_memberes from this list.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       *
       * @note you cannot vote against a committee_member, you can only vote for the committee_member
       *       or not vote for the committee_member.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param committee_member the name or id of the committee_member' owner account
       * @param approve true if you wish to vote in favor of that committee_member, false to 
       *                remove your vote in favor of that committee_member
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given committee_member
       */
    pair<tx_hash_type, signed_transaction> vote_for_committee_member(string voting_account,
                                                                     string committee_member,
                                                                     int64_t approve,
                                                                     bool broadcast = false);

    /** Vote for a given witness.
       *
       * An account can publish a list of all witnesses they approve of.  This 
       * command allows you to add or remove witnesses from this list.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       *
       * @note you cannot vote against a witness, you can only vote for the witness
       *       or not vote for the witness.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param witness the name or id of the witness' owner account
       * @param approve true if you wish to vote in favor of that witness, false to 
       *                remove your vote in favor of that witness
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given witness
       */
    pair<tx_hash_type, signed_transaction> vote_for_witness(string voting_account,
                                                            string witness,
                                                            int64_t approve,
                                                            bool broadcast = false);


    /** Signs a transaction.
       *
       * Given a fully-formed transaction that is only lacking signatures, this signs
       * the transaction with the necessary keys and optionally broadcasts the transaction
       * @param tx the unsigned transaction
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
    pair<tx_hash_type, signed_transaction> sign_transaction(signed_transaction tx, bool broadcast = false);

    /** Returns an uninitialized object representing a given blockchain operation.
       *
       * This returns a default-initialized object of the given type; it can be used 
       * during early development of the wallet when we don't yet have custom commands for
       * creating all of the operations the blockchain supports.  
       *
       * Any operation the blockchain supports can be created using the transaction builder's
       * \c add_operation_to_builder_transaction() , but to do that from the CLI you need to 
       * know what the JSON form of the operation looks like.  This will give you a template
       * you can fill in.  It's better than nothing.
       * 
       * @param operation_type the type of operation to return, must be one of the 
       *                       operations defined in `graphene/chain/operations.hpp`
       *                       (e.g., "global_parameters_update_operation")
       * @return a default-constructed operation of the given type
       */
    operation get_prototype_operation_by_name(string operation_type);
    operation get_prototype_operation_by_idx(uint index);

    /** Creates a transaction to propose a parameter change.
       *
       * Multiple parameters can be specified if an atomic change is
       * desired.
       *
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire.
       * @param changed_values The values to change; all other chain parameters are filled in with default values
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
    pair<tx_hash_type, signed_transaction> propose_parameter_change(
        const string &proposing_account,
        fc::time_point_sec expiration_time,
        const variant_object &changed_values,
        fc::optional<proposal_id_type>proposal_base,
        bool broadcast = false);

    /** Propose a fee change.
       * 
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire.
       * @param changed_values Map of operation type to new fee.  Operations may be specified by name or ID.
       *    The "scale" key changes the scale.  All other operations will maintain current values.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
    pair<tx_hash_type, signed_transaction> propose_fee_change(
        const string &proposing_account,
        fc::time_point_sec expiration_time,
        const variant_object &changed_values,
        fc::optional<proposal_id_type>proposal_base,
        bool broadcast = false);

    /** Approve or disapprove a proposal.
       *
       * @param fee_paying_account The account paying the fee for the op.
       * @param proposal_id The proposal to modify.
       * @param delta Members contain approvals to create or remove.  In JSON you can leave empty members undefined.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
    pair<tx_hash_type, signed_transaction> approve_proposal(
        const string &fee_paying_account,
        const string &proposal_id,
        const approval_delta &delta,
        bool broadcast /* = false */
    );

    order_book get_order_book(const string &base, const string &quote, unsigned limit = 50);

    void dbg_push_blocks(std::string src_filename, uint32_t count);
    void dbg_generate_blocks(std::string debug_wif_key, uint32_t count);
    void dbg_stream_json_objects(const std::string &filename);
    void dbg_update_object(fc::variant_object update);


    void network_add_nodes(const vector<string> &nodes);
    vector<variant> network_get_connected_peers();

   

    std::map<string, std::function<string(fc::variant, const fc::variants &)>> get_result_formatters() const;

    fc::signal<void(bool)> lock_changed;
    std::shared_ptr<detail::wallet_api_impl> my;
    void encrypt_keys();
    /*********************************************nico add*************************************************************************/
    pair<tx_hash_type, signed_transaction> create_contract(string owner, string name, public_key_type contract_authority, string data, bool broadcast = false); //nico add :: 创建合约
    pair<tx_hash_type, signed_transaction> revise_contract(string reviser, string contract_id_or_name, string data, bool broadcast = false);
    fc::variant get_account_contract_data(const string &account_id, string contract_id_or_name); //获取用户合约数据
    lua_map get_contract_public_data(string contract_id_or_name, lua_map filter);
    pair<tx_hash_type, signed_transaction> call_contract_function(string account_id_or_name, string contract_id_or_name, string function_name, vector<lua_types> value_list, bool broadcast = false);
    fc::optional<contract_object> get_contract(string contract_id_or_name);
    fc::optional<processed_transaction> get_transaction_by_id(string id);
    flat_set<public_key_type> get_signature_keys(const signed_transaction&trx);
    fc::optional<transaction_in_block_info> get_transaction_in_block_info(const string &id);
    pair<tx_hash_type, signed_transaction> adjustment_temporary_authorization(string account_id_or_name, string describe, fc::time_point_sec expiration_time, flat_map<public_key_type, weight_type> temporary_active, bool broadcast = false);
    chain_property_object get_chain_properties();
    processed_transaction validate_transaction(transaction_handle_type transaction_handle);
    pair<pair<tx_hash_type, pair<object_id_type, account_transaction_history_id_type>>, processed_transaction> get_account_top_transaction(const string account_id_or_name);
    pair<pair<tx_hash_type, pair<object_id_type, account_transaction_history_id_type>>, processed_transaction> get_account_transaction(const string account_id_or_name, account_transaction_history_id_type transaction_history_id);
    bool set_node_message_send_cache_size(const uint32_t &params);
    bool set_node_deduce_in_verification_mode(const bool &params);
    pair<tx_hash_type, signed_transaction> update_collateral_for_gas( account_id_type  mortgager,account_id_type  beneficiary,share_type  collateral,bool broadcast=false);
    vector<asset_restricted_object> list_asset_restricted_objects(const asset_id_type asset_id, restricted_enum restricted_type) const;
    pair<tx_hash_type, signed_transaction> asset_update_restricted_list(const string &asset_issuer, string  target_asset ,restricted_enum restricted_type,vector<object_id_type> restricted_list,bool isadd, bool broadcast);
    
    //void node_flush();
    // 注册游戏开发者

    // register as a non homogenesis asset creator
    pair<tx_hash_type, signed_transaction> register_nh_asset_creator(const string &fee_paying_account, bool broadcast = false);
    // create a world view
    pair<tx_hash_type, signed_transaction> create_world_view(const string &fee_paying_account, const string &world_view, bool broadcast = false);

    // propose relate to a world view
    pair<tx_hash_type, signed_transaction> propose_relate_world_view(const string &proposing_account,
                                                                     fc::time_point_sec expiration_time, const string &world_view_owner,
                                                                     const string &world_view, bool broadcast = false);

    // create non homogenesis asset
    pair<tx_hash_type, signed_transaction> create_nh_asset(const string &creator, const string &owner,
                                                           const string &asset_id, const string &world_view, const string &base_describe, bool broadcast = false);

    // list non homogenesis asset that created by this creator
    std::pair<vector<nh_asset_object>, uint32_t> list_nh_asset_by_creator(const string &nh_asset_creator,const string& world_view,uint32_t pagesize,uint32_t page);

    // list non homogenesis asset that can be used or owned under the account
    std::pair<vector<nh_asset_object>, uint32_t> list_account_nh_asset(const string &nh_asset_owner,
                                                                       const vector<string> &world_view_name_or_ids,
                                                                       uint32_t pagesize,
                                                                       uint32_t page,
                                                                       nh_asset_list_type list_type);

    // transfer non homogenesis asset to another account
    pair<tx_hash_type, signed_transaction> transfer_nh_asset(const string &from,const string &to,const string &nh_asset,bool broadcast = false);

    // get the details of a creator
    fc::optional<nh_asset_creator_object> get_nh_creator(const string &nh_asset_creator);

    // delete a non homogenesis asset
    pair<tx_hash_type, signed_transaction> delete_nh_asset(const string &fee_paying_account,const string &nh_asset,bool broadcast = false);

/*
    // realte non homogenesis assets
    // nh_asset_creator: the parent and child nht's creator
    // parent: parent nht
    // child:child nht
    // contract: contract ID
    // relate:true meens relate,and flase meens unrelate
    pair<tx_hash_type, signed_transaction> relate_nh_asset(const string &nh_asset_creator,const string &parent,
        const string &child,const string &contract_id_or_name,bool relate,bool broadcast = false);
*/
    // create nht order
    pair<tx_hash_type, signed_transaction> create_nh_asset_order(const string &seller,const string &otcaccount,const string &pending_fee_amount,
        const string &pending_fee_symbol,const string &nh_asset,const string &price_amount,const string &price_symbol,const string &memo,
        fc::time_point_sec expiration_time,bool broadcast = false);

    // list nht order
    std::pair<vector<nh_asset_order_object>, uint32_t> list_nh_asset_order(const string &world_view_name_or_id,const string &asset_symbols_or_id, 
        const string &base_describe = "",uint32_t pagesize = 10,uint32_t page = 1, bool is_ascending_order = true);

    // list nht order that created by this account
    std::pair<vector<nh_asset_order_object>, uint32_t> list_account_nh_asset_order(const string &nh_asset_order_owner,uint32_t pagesize,uint32_t page);

    // cancel nht order
    pair<tx_hash_type, signed_transaction> cancel_nh_asset_order(object_id_type order_id, bool broadcast = false);

    // fill nht order
    pair<tx_hash_type, signed_transaction> fill_nh_asset_order(const string &fee_paying_account,nh_asset_order_id_type order_id,bool broadcast = false);
    // create file
    // file_creator: file creator
    // file_name: file name
    // file_content: file content
    signed_transaction create_file(const string &file_creator,const string &file_name,const string &file_content,bool broadcast = false);

    // add accounts related to a file
    // file_creator: file creator
    // file_name_or_id: file's name or id
    // related_account: account who will be related
    signed_transaction add_file_relate_account(const string &file_creator,const string &file_name_or_id,const vector<string> &related_account,bool broadcast = false);

    // sign file
    // signature_account: signed account
    // file_name_or_id: file's name or id
    // signature: signature's content
    signed_transaction file_signature(const string &signature_account,const string &file_name_or_id,const string &signature,bool broadcast = false);

    // create a propose and propose that relate a file to parent file
    // file_creator: the creator of child file
    // parent_file_name: parent file's name or id
    // sub_file_name:child file's name or id
    // expiration_time: the expiration time of propose
    signed_transaction propose_relate_parent_file(const string &file_creator,const string &parent_file_name_or_id,const string &sub_file_name_or_id,
                                                    fc::time_point_sec expiration_time,bool broadcast = false);

    // look up file
    // file_name_or_ids: file's name or id
    fc::optional<file_object> lookup_file(const string &file_name_or_ids) const;

    // list files that created by this account
    // file_creator: the account who need look up
    map<string, file_id_type> list_account_created_file(const string &file_creator) const;
    // create a timed task
    // task_creator: the task's creator
    // start_time: the task's start time
    // expiration_time: the task's expiration time
    // execut_interval: the task's execut interval
    // target_execute_times: the task's target execute times
    // crontab_ops: task content
    pair<tx_hash_type, signed_transaction> create_crontab(const string &task_creator,fc::time_point_sec start_time, uint16_t execute_interval,
                                                              uint64_t target_execute_times,const vector<operation> &crontab_ops,bool broadcast = false);
    // cancel crontab
    // task_creator: the task's creator
    // task_id: the task's id
    pair<tx_hash_type, signed_transaction> cancel_crontab(const string &task_creator,object_id_type task_id,bool broadcast = false);
    pair<tx_hash_type, signed_transaction> crontab_builder_transaction(transaction_handle_type handle,const string &task_creator,
                                                fc::time_point_sec start_time,uint16_t execute_interval,uint64_t target_execute_times,bool broadcast=false);
    // list crontab(s) that created by this account
    // create_crontabr: the account who need look up
    vector<crontab_object> list_account_crontab(const string &crontab_creator, bool contain_normal = true, bool contain_suspended = true);
    // recover the suspended crontab
    pair<tx_hash_type, signed_transaction> recover_crontab(const string &crontab_creator, object_id_type crontab_id, fc::time_point_sec restart_time, bool broadcast = false);
    // zhangfan end
    int add_extern_sign_key(string wif);

    /**********************************************nico end*********************************************************************************/
};

} // namespace wallet
} // namespace graphene

FC_REFLECT(graphene::wallet::key_label, (label)(key))

FC_REFLECT(graphene::wallet::plain_keys, (keys)(checksum))

FC_REFLECT(graphene::wallet::wallet_data,
           (chain_id)(my_accounts)(cipher_keys)(extra_keys)(pending_account_registrations)(pending_witness_registrations)(labeled_keys)(ws_server)(ws_user)(ws_password))

FC_REFLECT(graphene::wallet::brain_key_info,
           (brain_priv_key)(wif_priv_key)(address_info)(pub_key))

FC_REFLECT(graphene::wallet::approval_delta,
           (active_approvals_to_add)(active_approvals_to_remove)(owner_approvals_to_add)(owner_approvals_to_remove)(key_approvals_to_add)(key_approvals_to_remove))

FC_REFLECT(graphene::wallet::worker_vote_delta,
           (vote_for)(vote_against)(vote_abstain))

FC_REFLECT_DERIVED(graphene::wallet::vesting_balance_object_with_info, (graphene::chain::vesting_balance_object),
                   (allowed_withdraw)(allowed_withdraw_time))

FC_REFLECT(graphene::wallet::operation_detail,
           (memo)(description)(op))

FC_API(graphene::wallet::wallet_api,
       (help)(gethelp)(info)(about)(begin_builder_transaction)(add_operation_to_builder_transaction)(replace_operation_in_builder_transaction)(preview_builder_transaction)(sign_builder_transaction)(propose_builder_transaction)(remove_builder_transaction)(is_new)(is_locked)(lock)(unlock)(set_password)(dump_private_keys)(list_my_accounts)(list_accounts)(list_account_balances)
       /*nico add*/
       //contract
       (call_contract_function)(create_contract)(revise_contract)(get_account_contract_data)(get_contract)(get_contract_public_data)
       //transaction
       (get_transaction_by_id)(get_transaction_in_block_info)(get_account_top_transaction)(get_account_transaction)(adjustment_temporary_authorization)(get_chain_properties)(validate_transaction)(add_extern_sign_key)
       //nh asset
       (register_nh_asset_creator)(create_world_view)(propose_relate_world_view)(create_nh_asset)(list_nh_asset_by_creator)(list_account_nh_asset)(transfer_nh_asset)(get_nh_creator)(delete_nh_asset)(create_nh_asset_order)(list_nh_asset_order)(cancel_nh_asset_order)(fill_nh_asset_order)(list_account_nh_asset_order)
       //(relate_nh_asset)
       //file
       (create_file)(add_file_relate_account)(file_signature)(propose_relate_parent_file)(list_account_created_file)(lookup_file)
       // crontab
       (create_crontab)(cancel_crontab)(list_account_crontab)(crontab_builder_transaction)(recover_crontab)(set_node_message_send_cache_size)(set_node_deduce_in_verification_mode)
       //gas
       (update_collateral_for_gas)(get_signature_keys)
       /*nico end*/
       (list_assets)(list_asset_restricted_objects)(asset_update_restricted_list)(import_key)(import_balance)(suggest_brain_key)(derive_owner_keys_from_brain_key)(register_account)(upgrade_account)(create_account_with_brain_key)(sell_asset)(sell)(buy)(borrow_asset)(cancel_order)(transfer)(transfer2)(get_transaction_id)(create_asset)(update_asset)(update_bitasset)(update_asset_feed_producers)(publish_asset_feed)(issue_asset)(get_asset)(get_bitasset_data)(reserve_asset)(global_settle_asset)(settle_asset)(bid_collateral)
       //(whitelist_account)
       (create_committee_member)(update_committee_member)(get_witness)(get_committee_member)(list_witnesses)(list_committee_members)(create_witness)(update_witness)(get_vesting_balances)(withdraw_vesting)(vote_for_committee_member)(vote_for_witness)(get_account)(get_account_id)(get_block)(get_account_count)(get_account_history)(get_relative_account_history)(get_collateral_bids)(is_public_key_registered)(get_market_history)(get_global_properties)(get_dynamic_global_properties)(get_object)(get_private_key)
       //
       (load_wallet_file)(normalize_brain_key)(get_limit_orders)(get_call_orders)(get_settle_orders)(save_wallet_file)(serialize_transaction)(sign_transaction)(get_prototype_operation_by_name)(get_prototype_operation_by_idx)(propose_parameter_change)(propose_fee_change)(approve_proposal)(dbg_push_blocks)(dbg_generate_blocks)(dbg_stream_json_objects)(dbg_update_object)(network_add_nodes)(network_get_connected_peers)(sign_memo)(read_memo)(set_key_label)(get_key_label)(get_public_key)(get_order_book))
