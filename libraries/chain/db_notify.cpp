
#include <graphene/chain/db_notify.hpp>
//using namespace fc;
//using namespace graphene::chain;

// TODO:  Review all of these, especially no-ops

namespace graphene
{
namespace chain
{

void operation_get_impacted_accounts(const operation &op, flat_set<account_id_type> &result)
{
  get_impacted_account_visitor vtor = get_impacted_account_visitor(result);
  op.visit(vtor);
  //result
}
void get_impacted_accounts_from_operation_reslut(const operation_result& ort, flat_set<account_id_type> &result)
{
  switch (ort.which())
  {
  case operation_result::tag<contract_result>::value:
  {
    auto cr = ort.get<contract_result>();
    contract_affected_type_visitor visitor;
    for (auto ac_itr : cr.contract_affecteds)
    {
      auto temp = ac_itr.visit(visitor);
      for (auto re : temp)
        result.insert(re);
    }
    break;
  }
  }
}

void transaction_get_impacted_accounts(const transaction &tx, flat_set<account_id_type> &result)
{
  for (const auto &op : tx.operations)
  {
    operation_get_impacted_accounts(op, result);
  }
}

void get_relevant_accounts(const object *obj, flat_set<account_id_type> &accounts) // 获取事件相关账户
{
  if (obj->id.space() == protocol_ids)
  {
    switch ((object_type)obj->id.type())
    {
    case null_object_type:
    case base_object_type:
    case OBJECT_TYPE_COUNT:
      return;
    case account_object_type:
    {
      accounts.insert(obj->id);
      break;
    }
    case asset_object_type:
    {
      const auto &aobj = dynamic_cast<const asset_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->issuer);
      break;
    }
    case force_settlement_object_type:
    {
      const auto &aobj = dynamic_cast<const force_settlement_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->owner);
      break;
    }
    case committee_member_object_type:
    {
      const auto &aobj = dynamic_cast<const committee_member_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->committee_member_account);
      break;
    }
    case witness_object_type:
    {
      const auto &aobj = dynamic_cast<const witness_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->witness_account);
      break;
    }
    case limit_order_object_type:
    {
      const auto &aobj = dynamic_cast<const limit_order_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->seller);
      break;
    }
    case call_order_object_type:
    {
      const auto &aobj = dynamic_cast<const call_order_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->borrower);
      break;
    }
    case custom_object_type:
    {
      break;
    }
    case proposal_object_type:
    {
      const auto &aobj = dynamic_cast<const proposal_object *>(obj);
      assert(aobj != nullptr);
      transaction_get_impacted_accounts(aobj->proposed_transaction, accounts);
      break;
    }
    case operation_history_object_type:
    {
      const auto &aobj = dynamic_cast<const operation_history_object *>(obj);
      assert(aobj != nullptr);
      operation_get_impacted_accounts(aobj->op, accounts);
      get_impacted_accounts_from_operation_reslut(aobj->result, accounts);
      break;
    }
    case vesting_balance_object_type:
    {
      const auto &aobj = dynamic_cast<const vesting_balance_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->owner);
      break;
    }
    case worker_object_type:
    {
      const auto &aobj = dynamic_cast<const worker_object *>(obj);
      assert(aobj != nullptr);
      if(aobj->beneficiary.valid())
        accounts.insert(*aobj->beneficiary);
      break;
    }
    case balance_object_type:
    {
      /** these are free from any accounts */
      break;
    }
#ifdef INCREASE_CONTRACT
    case contract_object_type:
    { //  获取contract_object相关账户
      const auto &aobj = dynamic_cast<const contract_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->owner);
      break;
    }
    case contract_data_type:
    { //  添加contract_data_type 事件通知
      const auto &aobj = dynamic_cast<const account_contract_data *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->owner);
      break;
    }
    case file_object_type:
    {
      const auto &file_obj = dynamic_cast<const file_object *>(obj);
      assert(file_obj != nullptr);
      for (const auto &a : file_obj->related_account)
      {
        accounts.insert(a);
      }
      break;
    }
    case crontab_object_type:
    {
      const auto &crontab_obj = dynamic_cast<const crontab_object *>(obj);
      assert(crontab_obj != nullptr);
      accounts.insert(crontab_obj->task_owner);
      break;
    }
#endif
    }
  }
  else if (obj->id.space() == implementation_ids)
  {
    switch ((impl_object_type)obj->id.type())
    {
    case impl_global_property_object_type:
      break;
    case impl_dynamic_global_property_object_type:
      break;
    case impl_contract_bin_code_type:
      break;
    case impl_asset_dynamic_data_type:
      break;
    case impl_asset_bitasset_data_type:
      break;
    case impl_account_balance_object_type:
    {
      const auto &aobj = dynamic_cast<const account_balance_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->owner);
      break;
    }
    case impl_account_statistics_object_type:
    {
      const auto &aobj = dynamic_cast<const account_statistics_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->owner);
      break;
    }
    case impl_transaction_object_type:
    {
      const auto &aobj = dynamic_cast<const transaction_object *>(obj);
      assert(aobj != nullptr);
      transaction_get_impacted_accounts(aobj->trx, accounts);
      break;
    }
    case impl_block_summary_object_type:
      break;
    case impl_account_transaction_history_object_type:
      break;
    case impl_chain_property_object_type:
      break;
    case impl_witness_schedule_object_type:
      break;
    case impl_budget_record_object_type:
      break;
    case impl_special_authority_object_type:
      break;
    case impl_collateral_bid_object_type:
    {
      const auto &aobj = dynamic_cast<const collateral_bid_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->bidder);
      break;
    }    
    }
  }
  else if (obj->id.space() == reserved_spaces::extension_id_for_nico)
  {
    switch ((extension_type_for_nico)obj->id.type())
    {
    case extension_type_for_nico::temporary_authority:
    {
      break; //临时权限受影响人，只影响自身账户，不影响其他人
    }
    case extension_type_for_nico::transaction_in_block_info_type:
    {
      break; //事物应用信息写入，不影响其他任何账户
    }
    case extension_type_for_nico::asset_restricted_object_type:{
      const auto &aobj = dynamic_cast<const asset_restricted_object *>(obj);
      assert(aobj != nullptr);
      if (aobj->restricted_type == restricted_enum::whitelist_authorities || 
            aobj->restricted_type == restricted_enum::blacklist_authorities)
      {
        accounts.insert(aobj->restricted_id);
      }
      break;
    }
    case extension_type_for_nico::unsuccessful_candidates_type:
    {
      break;
    }
    case extension_type_for_nico::collateral_for_gas_type:{
        const auto &aobj = dynamic_cast<const collateral_for_gas_object *>(obj);
        accounts.insert(aobj->beneficiary);
    }
    }
  }
  else if (obj->id.space() == reserved_spaces::nh_asset_protocol_ids)
  {
    switch ((nh_object_type)obj->id.type())
    {
    case nh_object_type::nh_asset_creator_object_type:
    {
      const auto &aobj = dynamic_cast<const nh_asset_creator_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->creator);
      break;
    }
    case nh_object_type::nh_asset_object_type:
    {
      const auto &aobj = dynamic_cast<const nh_asset_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->nh_asset_owner);
      break;
    }
    case nh_object_type::nh_asset_order_object_type:
    {
      const auto &aobj = dynamic_cast<const nh_asset_order_object *>(obj);
      assert(aobj != nullptr);
      accounts.insert(aobj->seller);
      break;
    }
    case nh_object_type::world_view_object_type:
    {
      break;
    }
    
    }
  }

} // end get_relevant_accounts( const object* obj, flat_set<account_id_type>& accounts )

void database::notify_changed_objects() // 消息通知对象改变
{
  try
  {
    if (_undo_db.enabled())
    {
      const auto &head_undo = _undo_db.head(); //最新的区块

      // New
      if (!new_objects.empty())
      {
        vector<object_id_type> new_ids;
        new_ids.reserve(head_undo.new_ids.size());
        flat_set<account_id_type> new_accounts_impacted;
        for (const auto &item : head_undo.new_ids)
        {
          new_ids.push_back(item);
          auto obj = find_object(item);
          if (obj != nullptr)
            get_relevant_accounts(obj, new_accounts_impacted);
        }

        new_objects(new_ids, new_accounts_impacted); //信号回调
      }

      // Changed
      if (!changed_objects.empty())
      {
        vector<object_id_type> changed_ids;
        changed_ids.reserve(head_undo.old_values.size());
        flat_set<account_id_type> changed_accounts_impacted;
        for (const auto &item : head_undo.old_values)
        {
          changed_ids.push_back(item.first);
          get_relevant_accounts(item.second.get(), changed_accounts_impacted);
        }

        changed_objects(changed_ids, changed_accounts_impacted);
      }

      // Removed
      if (!removed_objects.empty())
      {
        vector<object_id_type> removed_ids;
        removed_ids.reserve(head_undo.removed.size());
        vector<const object *> removed;
        removed.reserve(head_undo.removed.size());
        flat_set<account_id_type> removed_accounts_impacted;
        for (const auto &item : head_undo.removed)
        {
          removed_ids.emplace_back(item.first);
          auto obj = item.second.get();
          removed.emplace_back(obj);
          get_relevant_accounts(obj, removed_accounts_impacted);
        }

        removed_objects(removed_ids, removed, removed_accounts_impacted);
      }
    }
  }
  FC_CAPTURE_AND_LOG((0))
}

} // namespace chain
} // namespace graphene
