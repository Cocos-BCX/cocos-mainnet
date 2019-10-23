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
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene
{
namespace chain
{
class database;

/**
    * @class account_statistics_object
    * @ingroup object
    * @ingroup implementation
    *
    * This object contains regularly updated statistical data about an account. It is provided for the purpose of
    * separating the account data that changes frequently from the account data that is mostly static, which will
    * minimize the amount of data that must be backed up as part of the undo history everytime a transfer is made.
    */
class account_statistics_object : public graphene::db::abstract_object<account_statistics_object>
{
 public:
   static const uint8_t space_id = implementation_ids;
   static const uint8_t type_id = impl_account_statistics_object_type;

   account_id_type owner;

   /**
          * Keep the most recent operation as a root pointer to a linked list of the transaction history.
          */
   account_transaction_history_id_type most_recent_op;
   /** Total operations related to this account. */
   uint32_t total_ops = 0;
   /** Total operations related to this account that has been removed from the database. */
   uint32_t removed_ops = 0;

   /**
          * When calculating votes it is necessary to know how much is stored in orders (and thus unavailable for
          * transfers). Rather than maintaining an index of [asset,owner,order_id] we will simply maintain the running
          * total here and update it every time an order is created or modified.
          */
   share_type total_core_in_orders;
};

/**
    * @brief Tracks the balance of a single account/asset pair
    * @ingroup object
    *
    * This object is indexed on owner and asset_type so that black swan
    * events in asset_type can be processed quickly.
    */
class account_balance_object : public abstract_object<account_balance_object>
{
 public:
   static const uint8_t space_id = implementation_ids;
   static const uint8_t type_id = impl_account_balance_object_type;

   account_id_type owner;
   asset_id_type asset_type;
   share_type balance;

   asset get_balance() const { return asset(balance, asset_type); }
   void adjust_balance(const asset &delta);
};

/**
    * @brief This class represents an account on the object graph
    * @ingroup object
    * @ingroup protocol
    *
    * Accounts are the primary unit of authority on the graphene system. Users must have an account in order to use
    * assets, trade in the markets, vote for committee_members, etc.
    */
class account_object : public graphene::db::abstract_object<account_object>
{
 public:
   static const uint8_t space_id = protocol_ids;
   static const uint8_t type_id = account_object_type;

   /**
          * The time at which this account's membership expires.
          * If set to any time in the past, the account is a basic account.
          * If set to time_point_sec::maximum(), the account is a lifetime member.
          * If set to any time not in the past less than time_point_sec::maximum(), the account is an annual member.
          *
          * See @ref is_lifetime_member, @ref is_basic_account, @ref is_annual_member, and @ref is_member
          */
   time_point_sec membership_expiration_date;

   ///The account that paid the fee to register this account. Receives a percentage of referral rewards.
   account_id_type registrar;
   /// The account's name. This name must be unique among all account names on the graph. May not be empty.
   string name;
   optional<std::pair<witness_id_type,bool>> witness_status;
   optional<std::pair<committee_member_id_type,bool>> committee_status;

   /**
          * The owner authority represents absolute control over the account. Usually the keys in this authority will
          * be kept in cold storage, as they should not be needed very often and compromise of these keys constitutes
          * complete and irrevocable loss of the account. Generally the only time the owner authority is required is to
          * update the active authority.
          */
   authority owner;
   /// The owner authority contains the hot keys of the account. This authority has control over nearly all
   /// operations the account may perform.
   authority active;

   typedef account_options options_type;
   account_options options;

   /// The reference implementation records the account's statistics in a separate object. This field contains the
   /// ID of that object.
   account_statistics_id_type statistics;
   asset_locked_object asset_locked;

   /**
          * This is a set of all accounts which have 'whitelisted' this account. Whitelisting is only used in core
          * validation for the purpose of authorizing accounts to hold and transact in whitelisted assets. This
          * account cannot update this set, except by transferring ownership of the account, which will clear it. Other
          * accounts may add or remove their IDs from this set.
          */
   //flat_set<account_id_type> whitelisting_accounts;// 当前账户被哪些人列入了白名单

   /**
          * Optionally track all of the accounts this account has whitelisted or blacklisted, these should
          * be made Immutable so that when the account object is cloned no deep copy is required.  This state is
          * tracked for GUI display purposes.
          *
          * TODO: move white list tracking to its own multi-index container rather than having 4 fields on an
          * account.   This will scale better because under the current design if you whitelist 2000 accounts,
          * then every time someone fetches this account object they will get the full list of 2000 accounts.
          */
   ///@{
   //set<account_id_type> whitelisted_accounts;// 当前账户白名单列表中有哪些人
   //set<account_id_type> blacklisted_accounts;
   ///@}

   /**
          * This is a set of all accounts which have 'blacklisted' this account. Blacklisting is only used in core
          * validation for the purpose of forbidding accounts from holding and transacting in whitelisted assets. This
          * account cannot update this set, and it will be preserved even if the account is transferred. Other accounts
          * may add or remove their IDs from this set.
          */
   //flat_set<account_id_type> blacklisting_accounts;

   /**
          * Vesting balance which receives cashback_reward deposits.
          */
   optional<vesting_balance_id_type> cashback_gas;
   optional<vesting_balance_id_type> cashback_vb;
   optional<vesting_balance_id_type> cashback_vote;
   optional<special_authority> owner_special_authority = {};  // 创建一个类似与理事会的公共资产管理账户时，需要根据此项来确定owner权限来划分，可设置代币持有者　top N　来共同管理
   optional<special_authority> active_special_authority ={}; // 创建一个类似与理事会的公共资产管理账户时，需要根据此项来确定active权限来划分，可设置代币持有者　top N　来共同管理

   /**
          * This flag is set when the top_n logic sets both authorities,
          * and gets reset when authority or special_authority is set.
          */
   optional<uint8_t> top_n_control_flags;
   static const uint8_t top_n_control_owner = 1;
   static const uint8_t top_n_control_active = 2;

   bool has_special_authority() const
   {
      if (!(owner_special_authority.valid() && active_special_authority.valid()))
         return false;
      return (owner_special_authority->which() != special_authority::tag<no_special_authority>::value) || (active_special_authority->which() != special_authority::tag<no_special_authority>::value);
   }

   template <typename DB>
   const vesting_balance_object &cashback_balance(const DB &db) const
   {
      FC_ASSERT(cashback_gas);
      return db.get(*cashback_gas);
   }

   /// @return true if this is a lifetime member account; false otherwise.
   bool is_lifetime_member() const
   {
      return membership_expiration_date == time_point_sec::maximum();
   }
   /// @return true if this is a basic account; false otherwise.
   bool is_basic_account(time_point_sec now) const
   {
      return now > membership_expiration_date;
   }
   /// @return true if the account is an unexpired annual member; false otherwise.
   /// @note This method will return false for lifetime members.
   bool is_annual_member(time_point_sec now) const
   {
      return !is_lifetime_member() && !is_basic_account(now);
   }
   /// @return true if the account is an annual or lifetime member; false otherwise.
   bool is_member(time_point_sec now) const
   {
      return !is_basic_account(now);
   }

   account_id_type get_id() const { return id; }

    void pay_fee(database&db,asset fee)const;
};

/**
    *  @brief This secondary index will allow a reverse lookup of all accounts that a particular key or account
    *  is an potential signing authority.
    */
class account_member_index : public secondary_index
{
 public:
   virtual void object_inserted(const object &obj) override;
   virtual void object_removed(const object &obj) override;
   virtual void about_to_modify(const object &before) override;
   virtual void object_modified(const object &after) override;

   /** given an account or key, map it to the set of accounts that reference it in an active or owner authority */
   map<account_id_type, set<account_id_type>> account_to_account_memberships;
   map<public_key_type, set<account_id_type>> account_to_key_memberships;
   /** some accounts use address authorities in the genesis block */
   map<address, set<account_id_type>> account_to_address_memberships;

 protected:
   set<account_id_type> get_account_members(const account_object &a) const;
   set<public_key_type> get_key_members(const account_object &a) const;
   set<address> get_address_members(const account_object &a) const;

   set<account_id_type> before_account_members;
   set<public_key_type> before_key_members;
   set<address> before_address_members;
};



struct by_account_asset;
struct by_asset_balance;
/**
    * @ingroup object_index
    */
typedef multi_index_container<
    account_balance_object,
    indexed_by<
        ordered_unique<
            tag<by_id>, member<object, object_id_type, &object::id>>,
        ordered_unique<
            tag<by_account_asset>,
            composite_key<
                account_balance_object,
                member<account_balance_object, account_id_type, &account_balance_object::owner>,
                member<account_balance_object, asset_id_type, &account_balance_object::asset_type>>>,
        ordered_unique<
            tag<by_asset_balance>,
            composite_key<
                account_balance_object,
                member<account_balance_object, asset_id_type, &account_balance_object::asset_type>, //  键
                member<account_balance_object, share_type, &account_balance_object::balance>,
                member<account_balance_object, account_id_type, &account_balance_object::owner>>,
            composite_key_compare<        // 　比较方式　默认小->大
                std::less<asset_id_type>, //less 小->大
                std::greater<share_type>, //greater 大－>小
                std::less<account_id_type>>>>>
    account_balance_object_multi_index_type;

/**
    * @ingroup object_index
    */
typedef generic_index<account_balance_object, account_balance_object_multi_index_type> account_balance_index;

struct by_name
{
};

/**
    * @ingroup object_index
    */
typedef multi_index_container<
    account_object,
    indexed_by<
        ordered_unique<
            tag<by_id>,
            member<object, object_id_type, &object::id>>,
        ordered_unique<
            tag<by_name>,
            member<account_object, string, &account_object::name>>>>
    account_multi_index_type;

/**
    * @ingroup object_index
    */
typedef generic_index<account_object, account_multi_index_type> account_index;

} // namespace chain
} // namespace graphene

FC_REFLECT_DERIVED(graphene::chain::account_object,
                   (graphene::db::object),
                   (membership_expiration_date)(registrar)
                   (name)(witness_status)(committee_status)(owner)(active)(options)(statistics)(asset_locked)
                   (cashback_gas)(cashback_vb)(cashback_vote)(owner_special_authority)(active_special_authority)(top_n_control_flags))

FC_REFLECT_DERIVED(graphene::chain::account_balance_object,
                   (graphene::db::object),
                   (owner)(asset_type)(balance))

FC_REFLECT_DERIVED(graphene::chain::account_statistics_object,
                   (graphene::chain::object),
                   (owner)(most_recent_op)(total_ops)(removed_ops)(total_core_in_orders))
