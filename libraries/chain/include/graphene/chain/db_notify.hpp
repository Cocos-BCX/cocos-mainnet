#pragma once
#include <fc/container/flat.hpp>

#include <graphene/chain/protocol/authority.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/protocol/transaction.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/committee_member_object.hpp>

namespace graphene
{
namespace chain
{

void operation_get_impacted_accounts(
    const graphene::chain::operation &op,
    fc::flat_set<graphene::chain::account_id_type> &result);

void transaction_get_impacted_accounts(
    const graphene::chain::transaction &tx,
    fc::flat_set<graphene::chain::account_id_type> &result);
void get_impacted_accounts_from_operation_reslut(
    const graphene::chain::operation_result &ort,
    fc::flat_set<graphene::chain::account_id_type> &result);

struct get_impacted_account_visitor
{
      flat_set<account_id_type> &_impacted;
      get_impacted_account_visitor(flat_set<account_id_type> &impact) : _impacted(impact) {}
      typedef void result_type;

      void operator()(const transfer_operation &op)
      {
            _impacted.insert(op.to);
      }
      
      void operator()(const asset_claim_fees_operation &op) {}
      void operator()(const limit_order_create_operation &op) {}
      void operator()(const limit_order_cancel_operation &op)
      {
            _impacted.insert(op.fee_paying_account);
      }
      void operator()(const call_order_update_operation &op) {}
      void operator()(const bid_collateral_operation &op)
      {
            _impacted.insert(op.bidder);
      }
      void operator()(const fill_order_operation &op)
      {
            _impacted.insert(op.account_id);
      }
      void operator()(const execute_bid_operation &op)
      {
            _impacted.insert(op.bidder);
      }

      void operator()(const account_create_operation &op)
      {
            _impacted.insert(op.registrar);
            add_authority_accounts(_impacted, op.owner);
            add_authority_accounts(_impacted, op.active);
      }

      void operator()(const account_update_operation &op)
      {
            _impacted.insert(op.account);
            if (op.owner)
                  add_authority_accounts(_impacted, *(op.owner));
            if (op.active)
                  add_authority_accounts(_impacted, *(op.active));
      }
      /*
      void operator()(const account_whitelist_operation &op)
      {
            _impacted.insert(op.account_to_list);
      }
*/
      void operator()(const account_upgrade_operation &op) {}
      void operator()(const account_transfer_operation &op)
      {
            _impacted.insert(op.new_owner);
      }

      void operator()(const asset_create_operation &op) {}
      void operator()(const asset_update_operation &op)
      {
            if (op.new_issuer)
                  _impacted.insert(*(op.new_issuer));
      }

      void operator()(const asset_update_bitasset_operation &op) {}

      void operator()(const asset_update_restricted_operation &op)
      {
            if (op.restricted_type == restricted_enum::blacklist_authorities ||
                op.restricted_type == restricted_enum::whitelist_authorities)
            {
                  for (auto &temp : op.restricted_list)
                        _impacted.insert(account_id_type(temp));
            }
      }

      void operator()(const asset_update_feed_producers_operation &op) {}

      void operator()(const asset_issue_operation &op)
      {
            _impacted.insert(op.issue_to_account);
      }

      void operator()(const asset_reserve_operation &op) {}
      void operator()(const asset_settle_operation &op) {}
      void operator()(const asset_global_settle_operation &op) {}
      void operator()(const asset_publish_feed_operation &op) {}
      void operator()(const witness_create_operation &op)
      {
            _impacted.insert(op.witness_account);
      }
      void operator()(const witness_update_operation &op)
      {
            _impacted.insert(op.witness_account);
      }

      void operator()(const proposal_create_operation &op)
      {
            vector<authority> other;
            for (const auto &proposed_op : op.proposed_ops)
                  operation_get_required_authorities(proposed_op.op, _impacted, _impacted, other);
            for (auto &o : other)
                  add_authority_accounts(_impacted, o);
      }

      void operator()(const proposal_update_operation &op) {}
      void operator()(const proposal_delete_operation &op) {}

      void operator()(const committee_member_create_operation &op)
      {
            _impacted.insert(op.committee_member_account);
      }
      void operator()(const committee_member_update_operation &op)
      {
            _impacted.insert(op.committee_member_account);
      }
      void operator()(const committee_member_update_global_parameters_operation &op) {}

      void operator()(const vesting_balance_create_operation &op)
      {
            _impacted.insert(op.owner);
      }

      void operator()(const vesting_balance_withdraw_operation &op) {}
      void operator()(const worker_create_operation &op) {}
      void operator()(const balance_claim_operation &op) {}

      void operator()(const override_transfer_operation &op)
      {
            _impacted.insert(op.to);
            _impacted.insert(op.from);
            _impacted.insert(op.issuer);
      }

      void operator()(const asset_settle_cancel_operation &op)
      {
            _impacted.insert(op.account);
      }

      void operator()(const contract_create_operation &op) //nico 创建合约
      {
            //_impacted.insert( op.owner );
      }
      void operator()(const revise_contract_operation &op) //nico 修订合约
      {
            //_impacted.insert( op.owner );
      }

      void operator()(const call_contract_function_operation &op)
      { //nico 呼叫合约
            _impacted.insert(op.caller);
      }
      void operator()(const temporary_authority_change_operation &op)
      { //nico 临时权限
            // _impacted.insert( op.owner );
      }
      void operator()(const register_nh_asset_creator_operation &op) {}

      void operator()(const create_world_view_operation &op) {}

      void operator()(const relate_world_view_operation &op)
      {
            _impacted.insert(op.related_account);
      }

      void operator()(const create_nh_asset_operation &op)
      {
            _impacted.insert(op.owner);
      }

//     void operator()(const relate_nh_asset_operation &op) {}

      void operator()(const delete_nh_asset_operation &op) {}

      void operator()(const transfer_nh_asset_operation &op)
      {
            _impacted.insert(op.to);
      }

      void operator()(const create_nh_asset_order_operation &op)
      {
            _impacted.insert(op.otcaccount);
      }

      void operator()(const cancel_nh_asset_order_operation &op)
      {
            _impacted.insert(op.fee_paying_account);
      }

      void operator()(const fill_nh_asset_order_operation &op)
      {
            _impacted.insert(op.seller);
      }
      void operator()(const create_file_operation &op) {}
      void operator()(const add_file_relate_account_operation &op)
      {
            for (const auto &relate : op.related_account)
                  _impacted.insert(relate);
      }
      void operator()(const file_signature_operation &op) {}
      void operator()(const relate_parent_file_operation &op)
      {
            _impacted.insert(op.sub_file_owner);
      }
      void operator()(const crontab_create_operation &op) {}
      void operator()(const crontab_cancel_operation &op) {}
      void operator()(const crontab_recover_operation &op) {}
      void operator()(const update_collateral_for_gas_operation &op) {
             _impacted.insert(op.beneficiary);
      }
      void operator()(const account_authentication_operation&op){}
};

} // namespace chain
} // namespace graphene