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

#include <graphene/chain/database.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/block_summary_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/transaction_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/witness_schedule_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/contract_object.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/asset_evaluator.hpp>
#include <graphene/chain/balance_evaluator.hpp>
#include <graphene/chain/committee_member_evaluator.hpp>
#include <graphene/chain/market_evaluator.hpp>
#include <graphene/chain/proposal_evaluator.hpp>
#include <graphene/chain/transfer_evaluator.hpp>
#include <graphene/chain/vesting_balance_evaluator.hpp>
#include <graphene/chain/witness_evaluator.hpp>
#include <graphene/chain/worker_evaluator.hpp>

#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/temporary_authority_evaluator.hpp>

#include <graphene/chain/nh_asset_creator_evaluator.hpp>
#include <graphene/chain/nh_asset_evaluator.hpp>
#include <graphene/chain/nh_asset_order_evaluator.hpp>
#include <graphene/chain/world_view_evaluator.hpp>
#include <graphene/chain/gas_evaluator.hpp>

#include <graphene/chain/nh_asset_creator_object.hpp>
#include <graphene/chain/world_view_object.hpp>
#include <graphene/chain/nh_asset_object.hpp>
#include <graphene/chain/nh_asset_order_object.hpp>
#include <graphene/chain/file_object.hpp>
#include <graphene/chain/file_evaluator.hpp>
#include <graphene/chain/crontab_object.hpp>
#include <graphene/chain/crontab_evaluator.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>
#include <fc/crypto/digest.hpp>

#include <boost/algorithm/string.hpp>

namespace graphene
{
namespace chain
{

// C++ requires that static class variables declared and initialized
// in headers must also have a definition in a single source file,
// else linker errors will occur [1].
//
// The purpose of this source file is to collect such definitions in
// a single place.
//
// [1] http://stackoverflow.com/questions/8016780/undefined-reference-to-static-constexpr-char

const uint8_t account_object::space_id;
const uint8_t account_object::type_id;

const uint8_t asset_object::space_id;
const uint8_t asset_object::type_id;

const uint8_t block_summary_object::space_id;
const uint8_t block_summary_object::type_id;

const uint8_t call_order_object::space_id;
const uint8_t call_order_object::type_id;

const uint8_t committee_member_object::space_id;
const uint8_t committee_member_object::type_id;

const uint8_t force_settlement_object::space_id;
const uint8_t force_settlement_object::type_id;

const uint8_t global_property_object::space_id;
const uint8_t global_property_object::type_id;

const uint8_t limit_order_object::space_id;
const uint8_t limit_order_object::type_id;

const uint8_t operation_history_object::space_id;
const uint8_t operation_history_object::type_id;

const uint8_t proposal_object::space_id;
const uint8_t proposal_object::type_id;

const uint8_t transaction_object::space_id;
const uint8_t transaction_object::type_id;

const uint8_t vesting_balance_object::space_id;
const uint8_t vesting_balance_object::type_id;

const uint8_t witness_object::space_id;
const uint8_t witness_object::type_id;

const uint8_t worker_object::space_id;
const uint8_t worker_object::type_id;

void database::initialize_evaluators()
{
    _operation_evaluators.resize(255);
    register_evaluator<account_create_evaluator>();
    register_evaluator<account_update_evaluator>();
    register_evaluator<account_upgrade_evaluator>();
    //register_evaluator<account_whitelist_evaluator>();
    register_evaluator<committee_member_create_evaluator>();
    register_evaluator<committee_member_update_evaluator>();
    register_evaluator<committee_member_update_global_parameters_evaluator>();
    //register_evaluator<custom_evaluator>();
    register_evaluator<asset_create_evaluator>();
    register_evaluator<asset_issue_evaluator>();
    register_evaluator<asset_reserve_evaluator>();
    register_evaluator<asset_update_evaluator>();
    register_evaluator<asset_update_bitasset_evaluator>();
    register_evaluator<asset_update_feed_producers_evaluator>();
    register_evaluator<asset_settle_evaluator>();
    register_evaluator<asset_global_settle_evaluator>();
    //register_evaluator<assert_evaluator>();
    register_evaluator<limit_order_create_evaluator>();
    register_evaluator<limit_order_cancel_evaluator>();
    register_evaluator<call_order_update_evaluator>();
    register_evaluator<bid_collateral_evaluator>();
    register_evaluator<transfer_evaluator>();
    register_evaluator<asset_publish_feeds_evaluator>();
    register_evaluator<proposal_create_evaluator>();
    register_evaluator<proposal_update_evaluator>();
    register_evaluator<proposal_delete_evaluator>();
    register_evaluator<vesting_balance_create_evaluator>();
    register_evaluator<vesting_balance_withdraw_evaluator>();
    register_evaluator<witness_create_evaluator>();
    register_evaluator<witness_update_evaluator>();
    register_evaluator<worker_create_evaluator>();
    register_evaluator<balance_claim_evaluator>();
    register_evaluator<asset_claim_fees_evaluator>();
    register_evaluator<asset_update_restricted_evaluator>();
    register_evaluator<contract_create_evaluator>(); // 注册合约创建验证模块
    register_evaluator<revise_contract_evaluator>();
    register_evaluator<call_contract_function_evaluator>();
    register_evaluator<temporary_authority_change_evaluator>();

    register_evaluator<register_nh_asset_creator_evaluator>();
    register_evaluator<create_world_view_evaluator>();
    register_evaluator<relate_world_view_evaluator>();
    register_evaluator<create_nh_asset_evaluator>();
    register_evaluator<delete_nh_asset_evaluator>();
    register_evaluator<transfer_nh_asset_evaluator>();
    register_evaluator<create_nh_asset_order_evaluator>();
    register_evaluator<cancel_nh_asset_order_evaluator>();
    register_evaluator<fill_nh_asset_order_evaluator>();
    register_evaluator<create_file_evaluator>();
    register_evaluator<add_file_relate_account_evaluator>();
    register_evaluator<file_signature_evaluator>();
    register_evaluator<relate_parent_file_evaluator>();
    register_evaluator<crontab_create_evaluator>();
    register_evaluator<crontab_cancel_evaluator>();
    register_evaluator<crontab_recover_evaluator>();
    register_evaluator<update_collateral_for_gas_evaluator>();
}

void database::initialize_indexes()
{
    reset_indexes();
    _undo_db.set_max_size(GRAPHENE_MIN_UNDO_HISTORY);

    //Protocol object indexes
    add_index<primary_index<asset_index>>();
    add_index<primary_index<force_settlement_index>>();

    auto acnt_index = add_index<primary_index<account_index>>();
    acnt_index->add_secondary_index<account_member_index>();

    add_index<primary_index<committee_member_index>>();
    add_index<primary_index<witness_index>>();
    add_index<primary_index<limit_order_index>>();
    add_index<primary_index<call_order_index>>();

    auto prop_index = add_index<primary_index<proposal_index>>();
    prop_index->add_secondary_index<required_approval_index>();

    add_index<primary_index<vesting_balance_index>>();
    add_index<primary_index<worker_index>>();
    add_index<primary_index<balance_index>>();
#ifdef INCREASE_CONTRACT
    add_index<primary_index<contract_index>>();              // 合约对象数据表
    add_index<primary_index<account_contract_data_index>>(); // 用户合约数据表
    add_index<primary_index<temporary_active_index>>();      // 用户临时权限表
    add_index<primary_index<transaction_in_block_index>>();  // 交易事物应用表：存储交易事物的区块高度与交易事物在块中的数字索引
    add_index<primary_index<nh_asset_creator_index>>();
    add_index<primary_index<world_view_index>>();
    add_index<primary_index<nh_asset_index>>();
    add_index<primary_index<nh_asset_order_index>>();
    add_index<primary_index<file_index>>();
    add_index<primary_index<crontab_index>>();
    add_index<primary_index<asset_restricted_index>>();
    add_index<primary_index<contract_bin_code_index>>();
    add_index<primary_index<collateral_for_gas_index>>(); 
#endif
    //Implementation object indexes
    add_index<primary_index<transaction_index>>();
    add_index<primary_index<account_balance_index>>();
    add_index<primary_index<asset_bitasset_data_index>>();
    add_index<primary_index<simple_index<global_property_object>>>();
    add_index<primary_index<simple_index<dynamic_global_property_object>>>();
    add_index<primary_index<simple_index<account_statistics_object>>>();
    add_index<primary_index<simple_index<asset_dynamic_data_object>>>();
    add_index<primary_index<simple_index<block_summary_object>>>();
    add_index<primary_index<simple_index<chain_property_object>>>();
    add_index<primary_index<simple_index<witness_schedule_object>>>();
    add_index<primary_index<simple_index<budget_record_object>>>();
    add_index<primary_index<special_authority_index>>();
    add_index<primary_index<collateral_bid_index>>();
    add_index<primary_index<simple_index<unsuccessful_candidates_object>>>();
}

void database::init_genesis(const genesis_state_type &genesis_state)
{
    try
    {
        FC_ASSERT(genesis_state.initial_timestamp != time_point_sec(), "Must initialize genesis timestamp.");
        FC_ASSERT(genesis_state.initial_timestamp.sec_since_epoch() % GRAPHENE_DEFAULT_BLOCK_INTERVAL == 0,
                  "Genesis timestamp must be divisible by GRAPHENE_DEFAULT_BLOCK_INTERVAL.");
        FC_ASSERT(genesis_state.initial_witness_candidates.size() > 0,
                  "Cannot start a chain with zero witnesses.");
        FC_ASSERT(genesis_state.initial_active_witnesses <= genesis_state.initial_witness_candidates.size(),
                  "initial_active_witnesses is larger than the number of candidate witnesses.");

        _undo_db.disable();
        struct auth_inhibitor
        {
            auth_inhibitor(database &db) : db(db), old_flags(db.node_properties().skip_flags)
            {
                db.node_properties().skip_flags |= skip_authority_check;
            }
            ~auth_inhibitor()
            {
                db.node_properties().skip_flags = old_flags;
            }

          private:
            database &db;
            uint32_t old_flags;
        } inhibitor(*this);

        transaction_evaluation_state genesis_eval_state(this);

        // Create blockchain accounts
        fc::ecc::private_key null_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));
        create<account_balance_object>([](account_balance_object &b) {
            b.balance = GRAPHENE_MAX_SHARE_SUPPLY;
        });
        const account_object &committee_account =
            create<account_object>([&](account_object &n) {
                n.membership_expiration_date = time_point_sec::maximum();
                n.owner.weight_threshold = 1;
                n.active.weight_threshold = 1;
                n.name = "committee-account";
                n.statistics = create<account_statistics_object>([&](account_statistics_object &s) { s.owner = n.id; }).id;
            });
        FC_ASSERT(committee_account.get_id() == GRAPHENE_COMMITTEE_ACCOUNT);
        FC_ASSERT(create<account_object>([this](account_object& a) {
                    a.name = "committee-relaxed";
                    a.statistics = create<account_statistics_object>([&a](account_statistics_object& s){s.owner = a.id;}).id;
                    a.owner.weight_threshold = 1;
                    a.active.weight_threshold = 1;
                    a.registrar = GRAPHENE_RELAXED_COMMITTEE_ACCOUNT;
                    a.membership_expiration_date = time_point_sec::maximum();
                }).get_id() == GRAPHENE_RELAXED_COMMITTEE_ACCOUNT);
        FC_ASSERT(create<account_object>([this](account_object &a) {
                      a.name = "witness-account";
                      a.statistics = create<account_statistics_object>([&](account_statistics_object &s) { s.owner = a.id; }).id;
                      a.owner.weight_threshold = 1;
                      a.active.weight_threshold = 1;
                      a.registrar = GRAPHENE_WITNESS_ACCOUNT;
                      a.membership_expiration_date = time_point_sec::maximum();
                  }).get_id() == GRAPHENE_WITNESS_ACCOUNT);
        /*              
        FC_ASSERT(create<account_object>([this](account_object &a) {
                      a.name = "relaxed-committee-account";
                      a.statistics = create<account_statistics_object>([&](account_statistics_object &s) { s.owner = a.id; }).id;
                      a.owner.weight_threshold = 1;
                      a.active.weight_threshold = 1;
                      a.registrar  = GRAPHENE_RELAXED_COMMITTEE_ACCOUNT;
                      a.membership_expiration_date = time_point_sec::maximum();
                  })
                      .get_id() == GRAPHENE_RELAXED_COMMITTEE_ACCOUNT);
        */
        FC_ASSERT(create<account_object>([this](account_object &a) {
                      a.name = "null-account";
                      a.statistics = create<account_statistics_object>([&](account_statistics_object &s) { s.owner = a.id; }).id;
                      a.owner.weight_threshold = 1;
                      a.active.weight_threshold = 1;
                      a.registrar  = GRAPHENE_NULL_ACCOUNT;
                      a.membership_expiration_date = time_point_sec::maximum();
                  })
                      .get_id() == GRAPHENE_NULL_ACCOUNT);
        FC_ASSERT(create<account_object>([this](account_object &a) {
                      a.name = "temp-account";
                      a.statistics = create<account_statistics_object>([&](account_statistics_object &s) { s.owner = a.id; }).id;
                      a.owner.weight_threshold = 0;
                      a.active.weight_threshold = 0;
                      a.registrar  = GRAPHENE_TEMP_ACCOUNT;
                      a.membership_expiration_date = time_point_sec::maximum();
                  })
                      .get_id() == GRAPHENE_TEMP_ACCOUNT);
        // Create core asset
        const asset_object &core_asset =
            create<asset_object>([&](asset_object &a) {
                a.symbol = GRAPHENE_SYMBOL;
                a.options.max_supply = genesis_state.max_core_supply;
                a.precision = GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS;
                a.options.flags =0;
                a.options.issuer_permissions = 2;
                a.issuer = GRAPHENE_COMMITTEE_ACCOUNT;
                // a.options.core_exchange_rate.base=asset(1);
                //a.options.core_exchange_rate.quote=asset(1);
                a.dynamic_asset_data_id = create<asset_dynamic_data_object>([&](asset_dynamic_data_object &ad){
                                                                    ad.current_supply = GRAPHENE_MAX_SHARE_SUPPLY;}).id;
            });
        assert(core_asset.id == asset_id_type());
        const asset_object &gas_asset =
            create<asset_object>([&](asset_object &a) {
                a.symbol = "GAS";
                a.options.max_supply = genesis_state.max_core_supply;
                a.precision = GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS;
                a.options.flags = 0x8a;
                a.options.issuer_permissions = 0x8a;
                a.issuer = GRAPHENE_COMMITTEE_ACCOUNT;
                a.options.core_exchange_rate=price(asset(1),asset(1,GRAPHENE_ASSET_GAS));
                a.dynamic_asset_data_id =  create<asset_dynamic_data_object>([&](asset_dynamic_data_object &ad){
                                                                    ad.current_supply = 0;}).id;
            });
        assert(gas_asset.id == GRAPHENE_ASSET_GAS);
        chain_id_type chain_id = genesis_state.compute_chain_id();

        // Create global properties
        create<global_property_object>([&](global_property_object &p) {
            p.parameters = genesis_state.initial_parameters; // create
            //idump((p.parameters.maximum_run_time_ratio));
            // Set fees to zero initially, so that genesis initialization needs not pay them
            // We'll fix it at the end of the function
            p.parameters.current_fees->zero_all_fees();
        });
        create<dynamic_global_property_object>([&](dynamic_global_property_object &p) {
            p.time = genesis_state.initial_timestamp;
            p.dynamic_flags = 0;
            p.witness_budget = 0;
            p.recent_slots_filled = fc::uint128::max_value();
        });
        create<unsuccessful_candidates_object>([&](unsuccessful_candidates_object& unsuccessful_candidates){});
        FC_ASSERT((genesis_state.immutable_parameters.min_witness_count & 1) == 1, "min_witness_count must be odd");
        FC_ASSERT((genesis_state.immutable_parameters.min_committee_member_count & 1) == 1, "min_committee_member_count must be odd");
        FC_ASSERT(genesis_state.initial_parameters.witness_number_of_election>=genesis_state.immutable_parameters.min_witness_count&&
                  genesis_state.initial_witness_candidates.size()>=genesis_state.initial_parameters.witness_number_of_election);
        FC_ASSERT(genesis_state.initial_parameters.committee_number_of_election>=genesis_state.immutable_parameters.min_committee_member_count&&
                  genesis_state.initial_committee_candidates.size()>=genesis_state.initial_parameters.committee_number_of_election  );
        
        create<chain_property_object>([&](chain_property_object &p) {
            p.chain_id = chain_id;
            p.base_contract=genesis_state.initial_contract_base;
            p.immutable_parameters = genesis_state.immutable_parameters;
        });
        for (uint32_t i = 0; i <= 0x10000; i++)
            create<block_summary_object>([&](block_summary_object &) {}); // 创建摘要池(初始化1万个空的区块摘要)，后期会根据新的区块跟新摘要池

        // Create initial accounts
        for (const auto &account : genesis_state.initial_accounts)
        {
            account_create_operation cop;
            FC_ASSERT(is_valid_name(account.name), "account name (${name})  not a legitimate account name", ("name", account.name));
            cop.name = account.name;
            cop.registrar = GRAPHENE_TEMP_ACCOUNT;
            cop.owner = authority(1, account.owner_key, 1);
            if (account.active_key == public_key_type())
            {
                cop.active = cop.owner;
                cop.options.memo_key = account.owner_key;
            }
            else
            {
                cop.active = authority(1, account.active_key, 1);
                cop.options.memo_key = account.active_key;
            }
            account_id_type account_id(apply_operation(genesis_eval_state, cop).get<object_id_result>().result);

            if (account.is_lifetime_member)
            {
                account_upgrade_operation op;
                op.account_to_upgrade = account_id;
                op.upgrade_to_lifetime_member = true;
                apply_operation(genesis_eval_state, op);
            }
        }

        // Helper function to get account ID by name
        const auto &accounts_by_name = get_index_type<account_index>().indices().get<by_name>();
        auto get_account_id = [&accounts_by_name](const string &name) {
            auto itr = accounts_by_name.find(name);
            FC_ASSERT(itr != accounts_by_name.end(),
                      "Unable to find account '${acct}'. Did you forget to add a record for it to initial_accounts?",
                      ("acct", name));
            return itr->get_id();
        };

        // Helper function to get asset ID by symbol
        const auto &assets_by_symbol = get_index_type<asset_index>().indices().get<by_symbol>();
        const auto get_asset = [&assets_by_symbol](const string &symbol) {
            auto itr = assets_by_symbol.find(symbol);
            FC_ASSERT(itr != assets_by_symbol.end(),
                      "Unable to find asset '${sym}'. Did you forget to add a record for it to initial_assets?",
                      ("sym", symbol));
            return *itr;
        };

        map<asset_id_type, share_type> total_supplies;
        map<asset_id_type, share_type> total_debts;

        // Create initial assets
        for (const genesis_state_type::initial_asset_type &asset : genesis_state.initial_assets)
        {
            asset_id_type new_asset_id = get_index_type<asset_index>().get_next_id();
            total_supplies[new_asset_id] = 0;

            asset_dynamic_data_id_type dynamic_data_id;
            optional<asset_bitasset_data_id_type> bitasset_data_id;
            if (asset.is_bitasset)
            {
                int collateral_holder_number = 0;
                total_debts[new_asset_id] = 0;
                for (const auto &collateral_rec : asset.collateral_records)
                {
                    account_create_operation cop;
                    cop.name = asset.symbol + "-collateral-holder-" + std::to_string(collateral_holder_number);
                    boost::algorithm::to_lower(cop.name);
                    cop.registrar = GRAPHENE_TEMP_ACCOUNT;
                    cop.owner = authority(1, collateral_rec.owner, 1);
                    cop.active = cop.owner;
                    account_id_type owner_account_id = apply_operation(genesis_eval_state, cop).get<object_id_result>().result;

                    modify(owner_account_id(*this).statistics(*this), [&](account_statistics_object &o) {
                        o.total_core_in_orders = collateral_rec.collateral;
                    });

                    create<call_order_object>([&](call_order_object &c) {
                        c.borrower = owner_account_id;
                        c.collateral = collateral_rec.collateral;
                        c.debt = collateral_rec.debt;
                        c.call_price = price::call_price(chain::asset(c.debt, new_asset_id),
                                                         chain::asset(c.collateral, core_asset.id),
                                                         GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);
                    });

                    total_supplies[asset_id_type(0)] += collateral_rec.collateral;
                    total_debts[new_asset_id] += collateral_rec.debt;
                    ++collateral_holder_number;
                }

                bitasset_data_id = create<asset_bitasset_data_object>([&](asset_bitasset_data_object &b) {
                                       b.options.short_backing_asset = core_asset.id;
                                       b.options.minimum_feeds = GRAPHENE_DEFAULT_MINIMUM_FEEDS;
                                   }).id;
            }

           dynamic_data_id = create<asset_dynamic_data_object>([&](asset_dynamic_data_object &d) {
                                  d.accumulated_fees = asset.accumulated_fees;
                              }).id;

            total_supplies[new_asset_id] += asset.accumulated_fees;

            create<asset_object>([&](asset_object &a) {
                a.symbol = asset.symbol;
                a.options.description = asset.description;
                a.precision = asset.precision;
                string issuer_name = asset.issuer_name;
                a.issuer = get_account_id(issuer_name);
                a.options.max_supply = asset.max_supply;
                a.options.flags = witness_fed_asset;
                a.options.issuer_permissions = charge_market_fee | override_authority | white_list | transfer_restricted |
                                               (asset.is_bitasset ? disable_force_settle | global_settle | witness_fed_asset | committee_fed_asset : 0);
                a.dynamic_asset_data_id = dynamic_data_id;
                a.bitasset_data_id = bitasset_data_id;
            });
        }

        // Create initial balances
        share_type total_allocation;
        for (const auto &handout : genesis_state.initial_address_balances)
        {
            const auto asset_id = get_asset(handout.asset_symbol).get_id();
            create<balance_object>([&handout,asset_id](balance_object &b) {
                b.balance = asset(handout.amount, asset_id);
                b.owner = handout.owner;
            });

            total_supplies[asset_id] += handout.amount;
        }
        for (const auto &handout : genesis_state.initial_account_balances)
        {
            const auto asset = get_asset(handout.asset_symbol);
            FC_ASSERT(!asset.bitasset_data_id.valid(),"this asset is bitasset,asset:${asset}",("asset",handout.asset_symbol));
            const auto asset_id=asset.get_id();
            auto account =get_account_id(handout.owner_name);
            create<account_balance_object>([&handout,account, asset_id](account_balance_object &b) {
                b.balance = handout.amount;
                b.owner = account;
                b.asset_type=asset_id;
            });

            total_supplies[asset_id] += handout.amount;
        }

        // Create initial vesting balances
        for (const genesis_state_type::initial_vesting_balance_type &vest : genesis_state.initial_vesting_balances)
        {
            const auto asset_id = get_asset(vest.asset_symbol).get_id();
            create<balance_object>([&](balance_object &b) {
                b.owner = vest.owner;
                b.balance = asset(vest.amount, asset_id);

                linear_vesting_policy policy;
                policy.begin_timestamp = vest.begin_timestamp;
                policy.vesting_cliff_seconds = 0;
                policy.vesting_duration_seconds = vest.vesting_duration_seconds;
                policy.begin_balance = vest.begin_balance;

                b.vesting_policy = std::move(policy);
            });

            total_supplies[asset_id] += vest.amount;
        }

        if (total_supplies[asset_id_type(0)] > share_type(0))
        {
            adjust_balance(GRAPHENE_COMMITTEE_ACCOUNT, -get_balance(GRAPHENE_COMMITTEE_ACCOUNT, {}));
        }
        else
        {
            total_supplies[asset_id_type(0)] = GRAPHENE_MAX_SHARE_SUPPLY;
        }

        const auto &idx = get_index_type<asset_index>().indices().get<by_symbol>();
        auto it = idx.begin();
        bool has_imbalanced_assets = false;

        while (it != idx.end())
        {
            if (it->bitasset_data_id.valid())
            {
                auto supply_itr = total_supplies.find(it->id);
                auto debt_itr = total_debts.find(it->id);
                FC_ASSERT(supply_itr != total_supplies.end());
                FC_ASSERT(debt_itr != total_debts.end());
                if (supply_itr->second != debt_itr->second)
                {
                    has_imbalanced_assets = true;
                    elog("Genesis for asset ${aname} is not balanced\n"
                         "   Debt is ${debt}\n"
                         "   Supply is ${supply}\n",
                         ("debt", debt_itr->second)("supply", supply_itr->second));
                }
            }
            ++it;
        }
        FC_ASSERT(!has_imbalanced_assets);

        // Save tallied supplies
        for (const auto &item : total_supplies)
        {
            const auto asset_id = item.first;
            const auto total_supply = item.second;

            modify(get(asset_id), [&](asset_object &asset) {
                FC_ASSERT(total_supply<=asset.options.max_supply);
                modify(get(asset.dynamic_asset_data_id), [&](asset_dynamic_data_object &asset_data) {
                    asset_data.current_supply = total_supply;
                });
            });
        }

        // Create special witness account
        const witness_object &wit = create<witness_object>([&](witness_object &w) {});
        FC_ASSERT(wit.id == GRAPHENE_NULL_WITNESS);
        remove(wit);

        // Create initial witnesses
        std::for_each(genesis_state.initial_witness_candidates.begin(), genesis_state.initial_witness_candidates.end(),
                      [&](const genesis_state_type::initial_witness_type &witness) {
                          witness_create_operation op;
                          op.witness_account = get_account_id(witness.owner_name);
                          op.block_signing_key = witness.block_signing_key;
                          apply_operation(genesis_eval_state, op);
                      });

        // Create initial committee members
        std::for_each(genesis_state.initial_committee_candidates.begin(), genesis_state.initial_committee_candidates.end(),
                      [&](const genesis_state_type::initial_committee_member_type &member) {
                          committee_member_create_operation op;
                          op.committee_member_account = get_account_id(member.owner_name);
                          apply_operation(genesis_eval_state, op);
                      });

        // Create initial contract
        //uint32_t record_count = 0;//take off restriction ?
        auto create_contract_base = [&](const string &baseENV) {
            contract_create_operation op;
            op.owner = GRAPHENE_COMMITTEE_ACCOUNT;
            op.name = "contract.baseENV";
            op.data = baseENV;
            apply_operation(genesis_eval_state, std::move(op));
        };
        FC_ASSERT(genesis_state.initial_contract_base.size() > 0);
        create_contract_base(genesis_state.initial_contract_base);
        initialize_baseENV();
        // Set active witnesses
        modify(get_global_properties(), [&](global_property_object &p) {
            for (uint32_t i = 1; i <= genesis_state.initial_active_witnesses; ++i)
            {
                p.active_witnesses.insert(witness_id_type(i));
            }
        });

        // Enable fees
        modify(get_global_properties(), [&genesis_state](global_property_object &p) {
            p.parameters.current_fees = genesis_state.initial_parameters.current_fees;
            p.parameters.current_fees->full();
        });

        // Create witness scheduler
        create<witness_schedule_object>([&](witness_schedule_object &wso) {
            for (const witness_id_type &wid : get_global_properties().active_witnesses)
                wso.current_shuffled_witnesses.push_back(wid);
        });

        debug_dump();

        _undo_db.enable();
    }
    FC_CAPTURE_AND_RETHROW()
}
void database::initialize_baseENV()
{
    auto & contract_base=contract_id_type()(*this);
    auto &contract_base_code=contract_base.lua_code_b_id(*this);
    lua_getglobal(luaVM.mState, "baseENV");
    if (lua_isnil(luaVM.mState, -1))
    {
            lua_pop(luaVM.mState, 1);
            luaL_loadbuffer(luaVM.mState, contract_base_code.lua_code_b.data(), contract_base_code.lua_code_b.size(), contract_base.name.data());
            lua_setglobal(luaVM.mState, "baseENV");
    }
}

void database::initialize_luaVM()
{
    luaVM = graphene::chain::lua_scheduler(true);
    initialize_baseENV();
}

} // namespace chain
} // namespace graphene
