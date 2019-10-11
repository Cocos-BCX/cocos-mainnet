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
#include <fc/container/flat_fwd.hpp>
#include <fc/io/varint.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/optional.hpp>
#include <fc/safe.hpp>
#include <fc/container/flat.hpp>
#include <fc/string.hpp>
#include <fc/io/raw.hpp>
#include <fc/uint128.hpp>
#include <fc/static_variant.hpp>
#include <fc/smart_ref_fwd.hpp>

#include <memory>
#include <vector>
#include <deque>
#include <cstdint>
#include <graphene/chain/protocol/address.hpp>
#include <graphene/db/object_id.hpp>

#include <graphene/chain/protocol/config.hpp>

#include <graphene/db/object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene
{
namespace chain
{
using namespace graphene::db;

using std::deque;
using std::enable_shared_from_this;
using std::make_pair;
using std::map;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::tie;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using std::weak_ptr;

using fc::enum_type;
using fc::flat_map;
using fc::flat_set;
using fc::optional;
using fc::safe;
using fc::signed_int;
using fc::smart_ref;
using fc::static_variant;
using fc::time_point;
using fc::time_point_sec;
using fc::unsigned_int;
using fc::variant;
using fc::variant_object;
using fc::ecc::commitment_type;
using fc::ecc::range_proof_info;
using fc::ecc::range_proof_type;
struct void_t
{
};

typedef fc::ecc::private_key private_key_type;
typedef fc::sha256 chain_id_type;

enum asset_issuer_permission_flags
{
    charge_market_fee = 0x01,    /**< an issuer-specified percentage of all market trades in this asset is paid to the issuer */
    white_list = 0x02,           /**< accounts must be whitelisted in order to hold this asset */
    override_authority = 0x04,   /**< issuer may transfer asset back to himself */
    transfer_restricted = 0x08,  /**< require the issuer to be one party to every transfer */
    disable_force_settle = 0x10, /**< disable force settling */
    global_settle = 0x20,        /**< allow the bitasset issuer to force a global settling -- this may be set in permissions, but not flags */
    disable_issuer = 0x40, /**< allow the asset to be used with confidential transactions */
    witness_fed_asset = 0x80,    /**< allow the asset to be fed by witnesses */
    committee_fed_asset = 0x100  /**< allow the asset to be fed by the committee */
};
const static uint32_t ASSET_ISSUER_PERMISSION_MASK = charge_market_fee | white_list | override_authority | transfer_restricted | disable_force_settle | global_settle| disable_issuer | witness_fed_asset | committee_fed_asset;
const static uint32_t UIA_ASSET_ISSUER_PERMISSION_MASK = charge_market_fee | white_list | override_authority | transfer_restricted ;

enum reserved_spaces
{
    relative_protocol_ids = 0,
    protocol_ids = 1,
    implementation_ids = 2,
    extension_id_for_nico = 3,
    nh_asset_protocol_ids = 4,
    MARKET_HISTORY_SPACE_ID=5,
    RESERVED_SPACES_COUNT=6
};

inline bool is_relative(object_id_type o) { return o.space() == 0; }

/**
    *  List all object types from all namespaces here so they can
    *  be easily reflected and displayed in debug output.  If a 3rd party
    *  wants to extend the core code then they will have to change the
    *  packed_object::type field from enum_type to uint16 to avoid
    *  warnings when converting packed_objects to/from json.
    */
enum object_type
{
    null_object_type = 0,
    base_object_type = 1,
    account_object_type = 2,
    asset_object_type = 3,
    force_settlement_object_type = 4,
    committee_member_object_type = 5,
    witness_object_type = 6,
    limit_order_object_type = 7,
    call_order_object_type = 8,
    custom_object_type = 9,
    proposal_object_type = 10,
    operation_history_object_type = 11,
    crontab_object_type = 12,
    vesting_balance_object_type = 13,
    worker_object_type = 14,
    balance_object_type = 15,
#ifdef INCREASE_CONTRACT
    contract_object_type = 16,
    contract_data_type = 17,
    file_object_type = 18,
#endif
    OBJECT_TYPE_COUNT ///< Sentry value which contains the number of different object types
};
enum transaction_apply_mode
{
    push_mode = 0,
    production_block_mode = 1,
    apply_block_mode = 2,
    validate_transaction_mode = 3,
    invoke_mode = 4,
    just_try = 5
};
enum transaction_push_state
{
    from_me = 0,
    from_net = 1,
    re_push = 2,
    pop_block = 3
};
enum nht_affected_type
{
    transfer_from = 0,
    transfer_to = 1,
    modified = 2,
    create_by=3,
    create_for = 4,
    transfer_active_from = 5,
    transfer_active_to = 6,
    transfer_owner_from = 7,
    transfer_owner_to = 8,
    transfer_authority_from = 9,
    transfer_authority_to = 10,
    set_limit_list = 11,
    relate_nh_asset = 12
};

enum extension_type_for_nico
{
    temporary_authority = 0,
    transaction_in_block_info_type = 1,
    asset_restricted_object_type=2,
    unsuccessful_candidates_type=3,
    collateral_for_gas_type=4
};
enum nh_object_type
{
    nh_asset_creator_object_type, // 0
    world_view_object_type,   // 1
    nh_asset_object_type,      // 2
    nh_asset_order_object_type // 3
};

class nh_asset_creator_object;
class world_view_object;
class nh_asset_object;
class nh_asset_order_object;
class file_object;
class crontab_object;
class collateral_for_gas_object;
typedef object_id<nh_asset_protocol_ids, nh_asset_creator_object_type, nh_asset_creator_object> nh_asset_creator_id_type;
typedef object_id<nh_asset_protocol_ids, world_view_object_type, world_view_object> world_view_id_type;
typedef object_id<nh_asset_protocol_ids, nh_asset_object_type, nh_asset_object> nh_asset_id_type;
typedef object_id<nh_asset_protocol_ids, nh_asset_order_object_type, nh_asset_order_object> nh_asset_order_id_type;
typedef object_id< protocol_ids,     file_object_type,    file_object>       file_id_type;
typedef object_id< protocol_ids,     crontab_object_type,    crontab_object>       crontab_id_type;

enum impl_object_type
{
    impl_global_property_object_type = 0,
    impl_dynamic_global_property_object_type = 1,
    impl_contract_bin_code_type = 2, 
    impl_asset_dynamic_data_type = 3,
    impl_asset_bitasset_data_type = 4,
    impl_account_balance_object_type = 5,
    impl_account_statistics_object_type = 6,
    impl_transaction_object_type = 7,
    impl_block_summary_object_type = 8,
    impl_account_transaction_history_object_type = 9,
    impl_collateral_bid_object_type = 10,
    impl_chain_property_object_type = 11,
    impl_witness_schedule_object_type = 12,
    impl_budget_record_object_type = 13,
    impl_special_authority_object_type = 14
};

//typedef fc::unsigned_int            object_id_type;
//typedef uint64_t                    object_id_type;
class account_object;
class committee_member_object;
class witness_object;
class asset_object;
class force_settlement_object;
class limit_order_object;
class call_order_object;
class custom_object;
class proposal_object;
class operation_history_object;
class vesting_balance_object;
class worker_object;
class balance_object;
class blinded_balance_object;
class temporary_active_object;

typedef object_id<protocol_ids, account_object_type, account_object> account_id_type;
typedef object_id<protocol_ids, asset_object_type, asset_object> asset_id_type;
typedef object_id<protocol_ids, force_settlement_object_type, force_settlement_object> force_settlement_id_type;
typedef object_id<protocol_ids, committee_member_object_type, committee_member_object> committee_member_id_type;
typedef object_id<protocol_ids, witness_object_type, witness_object> witness_id_type;
typedef object_id<protocol_ids, limit_order_object_type, limit_order_object> limit_order_id_type;
typedef object_id<protocol_ids, call_order_object_type, call_order_object> call_order_id_type;
typedef object_id<protocol_ids, custom_object_type, custom_object> custom_id_type;
typedef object_id<protocol_ids, proposal_object_type, proposal_object> proposal_id_type;
typedef object_id<protocol_ids, operation_history_object_type, operation_history_object> operation_history_id_type;
typedef object_id<protocol_ids, vesting_balance_object_type, vesting_balance_object> vesting_balance_id_type;
typedef object_id<protocol_ids, worker_object_type, worker_object> worker_id_type;
typedef object_id<protocol_ids, balance_object_type, balance_object> balance_id_type;
#ifdef INCREASE_CONTRACT
class contract_object;
class account_contract_data;
class transaction_in_block_info;
typedef object_id<protocol_ids, contract_object_type, contract_object> contract_id_type;
typedef object_id<protocol_ids, contract_data_type, account_contract_data> contract_data_id_type;
typedef object_id<extension_id_for_nico, temporary_authority, temporary_active_object> temporary_active_object_id_tyep;
typedef object_id<extension_id_for_nico, transaction_in_block_info_type, transaction_in_block_info> transaction_in_block_info_id_type;
typedef object_id<extension_id_for_nico, collateral_for_gas_type , collateral_for_gas_object> collateral_for_gas_id_type;
#endif

// implementation types
class global_property_object;
class dynamic_global_property_object;
class asset_dynamic_data_object;
class asset_bitasset_data_object;
class account_balance_object;
class account_statistics_object;
class transaction_object;
class block_summary_object;
class account_transaction_history_object;
class chain_property_object;
class witness_schedule_object;
class budget_record_object;
class special_authority_object;
class collateral_bid_object;
struct by_greater_id{};
struct asset_restricted_object;
class contract_bin_code_object;
class unsuccessful_candidates_object;

typedef object_id<implementation_ids, impl_global_property_object_type, global_property_object> global_property_id_type;
typedef object_id<implementation_ids, impl_dynamic_global_property_object_type, dynamic_global_property_object> dynamic_global_property_id_type;
typedef object_id<implementation_ids, impl_asset_dynamic_data_type, asset_dynamic_data_object> asset_dynamic_data_id_type;
typedef object_id<implementation_ids, impl_asset_bitasset_data_type, asset_bitasset_data_object> asset_bitasset_data_id_type;
typedef object_id<implementation_ids, impl_account_balance_object_type, account_balance_object> account_balance_id_type;
typedef object_id<implementation_ids, impl_account_statistics_object_type, account_statistics_object> account_statistics_id_type;
typedef object_id<implementation_ids, impl_transaction_object_type, transaction_object> transaction_obj_id_type;
typedef object_id<implementation_ids, impl_block_summary_object_type, block_summary_object> block_summary_id_type;

typedef  object_id<implementation_ids, impl_contract_bin_code_type, contract_bin_code_object> contract_bin_code_id_type;

typedef object_id<implementation_ids,
                  impl_account_transaction_history_object_type,
                  account_transaction_history_object>
    account_transaction_history_id_type;
typedef object_id<implementation_ids, impl_chain_property_object_type, chain_property_object> chain_property_id_type;
typedef object_id<implementation_ids, impl_witness_schedule_object_type, witness_schedule_object> witness_schedule_id_type;
typedef object_id<implementation_ids, impl_budget_record_object_type, budget_record_object> budget_record_id_type;
typedef object_id<implementation_ids, impl_special_authority_object_type, special_authority_object> special_authority_id_type;
typedef object_id<implementation_ids, impl_collateral_bid_object_type, collateral_bid_object> collateral_bid_id_type;
typedef object_id<extension_id_for_nico, asset_restricted_object_type, asset_restricted_object> asset_restricted_id_type;
typedef object_id<extension_id_for_nico, unsuccessful_candidates_type,unsuccessful_candidates_object>unsuccessful_candidates_id_type;

typedef fc::array<char, GRAPHENE_MAX_ASSET_SYMBOL_LENGTH> symbol_type;
typedef fc::ripemd160 block_id_type;
typedef fc::ripemd160 checksum_type;
typedef fc::ripemd160 transaction_id_type;
typedef fc::sha256 digest_type;
typedef fc::sha512 digest512_type;
typedef fc::sha256 nh_hash_type;
typedef fc::sha256 tx_hash_type;
typedef fc::ecc::compact_signature signature_type;
typedef safe<int64_t> share_type;
typedef uint16_t weight_type;

struct public_key_type
{
    struct binary_key
    {
        binary_key() {}
        uint32_t check = 0;
        fc::ecc::public_key_data data;
    };
    fc::ecc::public_key_data key_data;
    public_key_type();
    public_key_type(const fc::ecc::public_key_data &data);
    public_key_type(const fc::ecc::public_key &pubkey);
    explicit public_key_type(const std::string &base58str);
    operator fc::ecc::public_key_data() const;
    operator fc::ecc::public_key() const;
    explicit operator std::string() const;
    friend bool operator==(const public_key_type &p1, const fc::ecc::public_key &p2);
    friend bool operator==(const public_key_type &p1, const public_key_type &p2);
    friend bool operator!=(const public_key_type &p1, const public_key_type &p2);
    // TODO: This is temporary for testing
    bool is_valid_v1(const std::string &base58str);
};

struct extended_public_key_type
{
    struct binary_key
    {
        binary_key() {}
        uint32_t check = 0;
        fc::ecc::extended_key_data data;
    };

    fc::ecc::extended_key_data key_data;

    extended_public_key_type();
    extended_public_key_type(const fc::ecc::extended_key_data &data);
    extended_public_key_type(const fc::ecc::extended_public_key &extpubkey);
    explicit extended_public_key_type(const std::string &base58str);
    operator fc::ecc::extended_public_key() const;
    explicit operator std::string() const;
    friend bool operator==(const extended_public_key_type &p1, const fc::ecc::extended_public_key &p2);
    friend bool operator==(const extended_public_key_type &p1, const extended_public_key_type &p2);
    friend bool operator!=(const extended_public_key_type &p1, const extended_public_key_type &p2);
};

struct extended_private_key_type
{
    struct binary_key
    {
        binary_key() {}
        uint32_t check = 0;
        fc::ecc::extended_key_data data;
    };

    fc::ecc::extended_key_data key_data;

    extended_private_key_type();
    extended_private_key_type(const fc::ecc::extended_key_data &data);
    extended_private_key_type(const fc::ecc::extended_private_key &extprivkey);
    explicit extended_private_key_type(const std::string &base58str);
    operator fc::ecc::extended_private_key() const;
    explicit operator std::string() const;
    friend bool operator==(const extended_private_key_type &p1, const fc::ecc::extended_private_key &p2);
    friend bool operator==(const extended_private_key_type &p1, const extended_private_key_type &p2);
    friend bool operator!=(const extended_private_key_type &p1, const extended_private_key_type &p2);
};
} // namespace chain
} // namespace graphene

namespace fc
{
void to_variant(const graphene::chain::public_key_type &var, fc::variant &vo);
void from_variant(const fc::variant &var, graphene::chain::public_key_type &vo);
void to_variant(const graphene::chain::extended_public_key_type &var, fc::variant &vo);
void from_variant(const fc::variant &var, graphene::chain::extended_public_key_type &vo);
void to_variant(const graphene::chain::extended_private_key_type &var, fc::variant &vo);
void from_variant(const fc::variant &var, graphene::chain::extended_private_key_type &vo);
} // namespace fc

FC_REFLECT(graphene::chain::public_key_type, (key_data))
FC_REFLECT(graphene::chain::public_key_type::binary_key, (data)(check))
FC_REFLECT(graphene::chain::extended_public_key_type, (key_data))
FC_REFLECT(graphene::chain::extended_public_key_type::binary_key, (check)(data))
FC_REFLECT(graphene::chain::extended_private_key_type, (key_data))
FC_REFLECT(graphene::chain::extended_private_key_type::binary_key, (check)(data))

FC_REFLECT_ENUM(graphene::chain::object_type,
                (null_object_type)(base_object_type)(account_object_type)(force_settlement_object_type)(asset_object_type)(committee_member_object_type)(witness_object_type)(limit_order_object_type)(call_order_object_type)(custom_object_type)(proposal_object_type)(operation_history_object_type)(vesting_balance_object_type)(worker_object_type)(balance_object_type)
#ifdef INCREASE_CONTRACT
                    (contract_object_type)(contract_data_type)(file_object_type)(crontab_object_type)
#endif
                        (OBJECT_TYPE_COUNT))
FC_REFLECT_ENUM(graphene::chain::impl_object_type,
                (impl_global_property_object_type)(impl_dynamic_global_property_object_type)(impl_contract_bin_code_type)(impl_asset_dynamic_data_type)(impl_asset_bitasset_data_type)(impl_account_balance_object_type)(impl_account_statistics_object_type)(impl_transaction_object_type)
                (impl_block_summary_object_type)(impl_account_transaction_history_object_type)(impl_chain_property_object_type)(impl_witness_schedule_object_type)(impl_budget_record_object_type)(impl_special_authority_object_type)
                (impl_collateral_bid_object_type))
FC_REFLECT_ENUM(graphene::chain::extension_type_for_nico,
                (temporary_authority)(transaction_in_block_info_type)(asset_restricted_object_type)(unsuccessful_candidates_type)(collateral_for_gas_type))
FC_REFLECT_ENUM(graphene::chain::nh_object_type,
                (nh_asset_creator_object_type)(world_view_object_type)(nh_asset_object_type)(nh_asset_order_object_type))
FC_REFLECT_ENUM(graphene::chain::transaction_apply_mode,
                (push_mode)(production_block_mode)(apply_block_mode)(validate_transaction_mode)(invoke_mode)(just_try))
FC_REFLECT_ENUM(graphene::chain::transaction_push_state,
                (from_me)(from_net)(re_push)(pop_block))
FC_REFLECT_ENUM(graphene::chain::nht_affected_type,
                (transfer_from)(transfer_to)(modified)(create_by)(create_for)(transfer_active_from)(transfer_active_to)(transfer_owner_from)(transfer_owner_to)(transfer_authority_from)(transfer_authority_to)(set_limit_list)(relate_nh_asset))
FC_REFLECT_TYPENAME(graphene::chain::share_type)
/******************nico add******************************/
FC_REFLECT_TYPENAME(graphene::chain::contract_id_type)
FC_REFLECT_TYPENAME(graphene::chain::contract_data_id_type)
FC_REFLECT_TYPENAME(graphene::chain::temporary_active_object_id_tyep)
FC_REFLECT_TYPENAME(graphene::chain::transaction_in_block_info_id_type)
FC_REFLECT_TYPENAME(graphene::chain::collateral_for_gas_id_type)
FC_REFLECT_TYPENAME( graphene::chain::nh_asset_creator_id_type )
FC_REFLECT_TYPENAME( graphene::chain::world_view_id_type )
FC_REFLECT_TYPENAME( graphene::chain::nh_asset_id_type )
FC_REFLECT_TYPENAME( graphene::chain::nh_asset_order_id_type )
FC_REFLECT_TYPENAME( graphene::chain::file_id_type )
FC_REFLECT_TYPENAME( graphene::chain::crontab_id_type )
FC_REFLECT_TYPENAME( graphene::chain::contract_bin_code_id_type )

/**********************************************/
FC_REFLECT_TYPENAME(graphene::chain::account_id_type)
FC_REFLECT_TYPENAME(graphene::chain::asset_id_type)
FC_REFLECT_TYPENAME(graphene::chain::force_settlement_id_type)
FC_REFLECT_TYPENAME(graphene::chain::committee_member_id_type)
FC_REFLECT_TYPENAME(graphene::chain::witness_id_type)
FC_REFLECT_TYPENAME(graphene::chain::limit_order_id_type)
FC_REFLECT_TYPENAME(graphene::chain::call_order_id_type)
FC_REFLECT_TYPENAME(graphene::chain::custom_id_type)
FC_REFLECT_TYPENAME(graphene::chain::proposal_id_type)
FC_REFLECT_TYPENAME(graphene::chain::operation_history_id_type)
FC_REFLECT_TYPENAME(graphene::chain::vesting_balance_id_type)
FC_REFLECT_TYPENAME(graphene::chain::worker_id_type)
FC_REFLECT_TYPENAME(graphene::chain::balance_id_type)
FC_REFLECT_TYPENAME(graphene::chain::global_property_id_type)
FC_REFLECT_TYPENAME(graphene::chain::dynamic_global_property_id_type)
FC_REFLECT_TYPENAME(graphene::chain::asset_dynamic_data_id_type)
FC_REFLECT_TYPENAME(graphene::chain::asset_bitasset_data_id_type)
FC_REFLECT_TYPENAME(graphene::chain::account_balance_id_type)
FC_REFLECT_TYPENAME(graphene::chain::account_statistics_id_type)
FC_REFLECT_TYPENAME(graphene::chain::transaction_obj_id_type)
FC_REFLECT_TYPENAME(graphene::chain::block_summary_id_type)
FC_REFLECT_TYPENAME(graphene::chain::account_transaction_history_id_type)
FC_REFLECT_TYPENAME(graphene::chain::budget_record_id_type)
FC_REFLECT_TYPENAME(graphene::chain::special_authority_id_type)
FC_REFLECT_TYPENAME(graphene::chain::collateral_bid_id_type)
FC_REFLECT_TYPENAME(graphene::chain::asset_restricted_id_type)
FC_REFLECT(graphene::chain::void_t, )

FC_REFLECT_ENUM(graphene::chain::asset_issuer_permission_flags,
                (charge_market_fee)(white_list)(transfer_restricted)(override_authority)(disable_force_settle)(global_settle)(disable_issuer)(witness_fed_asset)(committee_fed_asset))
