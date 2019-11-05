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
#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/ext.hpp>
#include <graphene/chain/protocol/special_authority.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/vote.hpp>

namespace graphene
{
namespace chain
{

bool is_valid_name(const string &s);
bool is_cheap_name(const string &n);

/// These are the fields which can be updated by the active authority.
struct account_options
{
  /// The memo key is the key this account will typically use to encrypt/sign transaction memos and other non-
  /// validated account activities. This field is here to prevent confusion if the active authority has zero or
  /// multiple keys in it.
  public_key_type memo_key;
  /// This is the list of vote IDs this account votes for. The weight of these votes is determined by this
  /// account's balance of core asset.
  flat_set<vote_id_type> votes;
  extensions_type extensions;
  //flat_set<std::string>   extensions;
  void validate() const;
};
struct asset_locked_object
{
  map<asset_id_type,share_type> locked_total;
  map<contract_id_type,map<asset_id_type,share_type>> contract_lock_details;
  optional<asset> committee_freeze;
  optional<asset> witness_freeze;
  optional<asset> vote_for_committee;
  optional<asset> vote_for_witness;
  };
/**
    *  @ingroup operations
    */
struct account_create_operation : public base_operation
{
  struct ext
  {
    optional<void_t> null_ext;
    optional<special_authority> owner_special_authority;
    optional<special_authority> active_special_authority;
  };

  struct fee_parameters_type
  {
    uint64_t basic_fee = 5 * GRAPHENE_BLOCKCHAIN_PRECISION;      ///< the cost to register the cheapest non-free account
    uint64_t premium_fee = 2000 * GRAPHENE_BLOCKCHAIN_PRECISION; ///< the cost to register the cheapest non-free account
    uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
  };
  /// This account pays the fee. Must be a lifetime member.
  account_id_type registrar;

  string name;
  authority owner;
  authority active;

  account_options options;
  extension<ext> extensions;

  account_id_type fee_payer() const { return registrar; }
  void validate() const;
  share_type calculate_fee(const fee_parameters_type &) const;

  void get_required_active_authorities(flat_set<account_id_type> &a) const
  {
    // registrar should be required anyway as it is the fee_payer(), but we insert it here just to be sure
    a.insert(registrar);
  }
};

/**
    * @ingroup operations
    * @brief Update an existing account
    *
    * This operation is used to update an existing account. It can be used to update the authorities, or adjust the options on the account.
    * See @ref account_object::options_type for the options which may be updated.
    */
struct account_update_operation : public base_operation
{
  struct ext
  {
    optional<void_t> null_ext;
    optional<special_authority> owner_special_authority;
    optional<special_authority> active_special_authority;
  };

  struct fee_parameters_type
  {
    share_type fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
    uint32_t price_per_kbyte = GRAPHENE_BLOCKCHAIN_PRECISION;
  };

  optional<pair<uint32_t,asset>> lock_with_vote;
  /// The account to update
  account_id_type account;

  /// New owner authority. If set, this operation requires owner authority to execute.
  optional<authority> owner;
  /// New active authority. This can be updated by the current active authority.
  optional<authority> active;

  /// New account options
  optional<account_options> new_options;
  extension<ext> extensions;

  account_id_type fee_payer() const { return account; }
  void validate() const;
  share_type calculate_fee(const fee_parameters_type &k) const;

  bool is_owner_update() const
  {
    return owner || extensions.value.owner_special_authority.valid();
  }

  void get_required_owner_authorities(flat_set<account_id_type> &a) const
  {
      a.insert(account);
  }

  void get_required_active_authorities(flat_set<account_id_type> &a) const
  {
      a.insert(account);
  }
};

/**
    * @brief This operation is used to whitelist and blacklist accounts, primarily for transacting in whitelisted assets
    * @ingroup operations
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
    * This operation requires authorizing_account's signature, but not account_to_list's. The fee is paid by
    * authorizing_account.
    */
/*
struct account_whitelist_operation : public base_operation
{
  struct fee_parameters_type
  {
    share_type fee = 300000;
  };
  enum account_listing
  {
    no_listing = 0x0,                                    ///< No opinion is specified about this account
    white_listed = 0x1,                                  ///< This account is whitelisted, but not blacklisted
    black_listed = 0x2,                                  ///< This account is blacklisted, but not whitelisted
    white_and_black_listed = white_listed | black_listed ///< This account is both whitelisted and blacklisted
  };

  /// Paid by authorizing_account
  asset fee;
  /// The account which is specifying an opinion of another account
  account_id_type authorizing_account;
  /// The account being opined about
  account_id_type account_to_list;
  /// The new white and blacklist status of account_to_list, as determined by authorizing_account
  /// This is a bitfield using values defined in the account_listing enum
  uint8_t new_listing = no_listing;
  extensions_type extensions;

  account_id_type fee_payer() const { return authorizing_account; }
  void validate() const
  {
    FC_ASSERT(new_listing < 0x4);
  }
};
*/

/**
    * @brief Manage an account's membership status
    * @ingroup operations
    *
    * This operation is used to upgrade an account to a member, or renew its subscription. If an account which is an
    * unexpired annual subscription member publishes this operation with @ref upgrade_to_lifetime_member set to false,
    * the account's membership expiration date will be pushed backward one year. If a basic account publishes it with
    * @ref upgrade_to_lifetime_member set to false, the account will be upgraded to a subscription member with an
    * expiration date one year after the processing time of this operation.
    *
    * Any account may use this operation to become a lifetime member by setting @ref upgrade_to_lifetime_member to
    * true. Once an account has become a lifetime member, it may not use this operation anymore.
    */
struct account_upgrade_operation : public base_operation
{
  struct fee_parameters_type
  {
    uint64_t membership_annual_fee = 2000 * GRAPHENE_BLOCKCHAIN_PRECISION;
    uint64_t membership_lifetime_fee = 10000 * GRAPHENE_BLOCKCHAIN_PRECISION; ///< the cost to upgrade to a lifetime member
  };
  /// The account to upgrade; must not already be a lifetime member
  account_id_type account_to_upgrade;
  /// If true, the account will be upgraded to a lifetime member; otherwise, it will add a year to the subscription
  bool upgrade_to_lifetime_member = false;
  extensions_type extensions;

  account_id_type fee_payer() const { return account_to_upgrade; }
  void validate() const;
  share_type calculate_fee(const fee_parameters_type &k) const;
};

/**
    * @brief transfers the account to another account while clearing the white list
    * @ingroup operations
    *
    * In theory an account can be transferred by simply updating the authorities, but that kind
    * of transfer lacks semantic meaning and is more often done to rotate keys without transferring
    * ownership.   This operation is used to indicate the legal transfer of title to this account and
    * a break in the operation history.
    *
    * The account_id's owner/active/voting/memo authority should be set to new_owner
    *
    * This operation will clear the account's whitelist statuses, but not the blacklist statuses.
    */
struct account_transfer_operation : public base_operation
{
  struct fee_parameters_type
  {
    uint64_t fee = 500 * GRAPHENE_BLOCKCHAIN_PRECISION;
  };
  account_id_type account_id;
  account_id_type new_owner;
  extensions_type extensions;

  account_id_type fee_payer() const { return account_id; }
  void validate() const;
};

struct account_authentication_operation : public base_operation
{
  struct fee_parameters_type
  {
    uint64_t fee = 500 * GRAPHENE_BLOCKCHAIN_PRECISION;
  };
  account_id_type account_id;
  string data;
  extensions_type extensions;
  account_id_type fee_payer() const { return account_id; }
  void validate() const
  {
    FC_ASSERT(fc::raw::pack_size(*this)<=102400);
  };

};

} // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::account_options, (memo_key)(votes)(extensions))
FC_REFLECT(graphene::chain::account_authentication_operation::fee_parameters_type, (fee))
FC_REFLECT(graphene::chain::account_authentication_operation, (account_id)(data)(extensions))
FC_REFLECT(graphene::chain::asset_locked_object, (locked_total)(contract_lock_details)(committee_freeze)(witness_freeze)(vote_for_committee)(vote_for_witness))
/*
FC_REFLECT_ENUM(graphene::chain::account_whitelist_operation::account_listing,
                (no_listing)(white_listed)(black_listed)(white_and_black_listed))
*/
FC_REFLECT(graphene::chain::account_create_operation::ext, (null_ext)(owner_special_authority)(active_special_authority))
FC_REFLECT(graphene::chain::account_create_operation,
           (registrar)(name)(owner)(active)(options)(extensions))

FC_REFLECT(graphene::chain::account_update_operation::ext, (null_ext)(owner_special_authority)(active_special_authority))
FC_REFLECT(graphene::chain::account_update_operation,
           (lock_with_vote)(account)(owner)(active)(new_options)(extensions))

FC_REFLECT(graphene::chain::account_upgrade_operation,
           (account_to_upgrade)(upgrade_to_lifetime_member)(extensions))
/*
FC_REFLECT(graphene::chain::account_whitelist_operation, (fee)(authorizing_account)(account_to_list)(new_listing)(extensions))
*/
FC_REFLECT(graphene::chain::account_create_operation::fee_parameters_type, (basic_fee)(premium_fee)(price_per_kbyte))
/*
FC_REFLECT(graphene::chain::account_whitelist_operation::fee_parameters_type, (fee))
*/
FC_REFLECT(graphene::chain::account_update_operation::fee_parameters_type, (fee)(price_per_kbyte))
FC_REFLECT(graphene::chain::account_upgrade_operation::fee_parameters_type, (membership_annual_fee)(membership_lifetime_fee))
FC_REFLECT(graphene::chain::account_transfer_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::account_transfer_operation, (account_id)(new_owner)(extensions))
