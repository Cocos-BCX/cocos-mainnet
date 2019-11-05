#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_function_register_scheduler.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
namespace graphene
{
namespace chain
{

const account_object &register_scheduler::get_account(string name_or_id)
{
    try
    {
        return db.get_account(name_or_id);
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
        std::terminate();
    }
}
void register_scheduler::transfer_by_contract(account_id_type from, account_id_type to, asset token, contract_result &result, bool enable_logger)
{
    try
    {
        FC_ASSERT(token.asset_id != GRAPHENE_ASSET_GAS);
        FC_ASSERT(from != to, "It's no use transferring money to yourself");
        const account_object &from_account = from(db);
        const account_object &to_account = to(db);
        const asset_object &asset_type = token.asset_id(db);
        try
        {
            FC_ASSERT(db.get_balance(from_account, asset_type).amount >= token.amount,
                      "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'",
                      ("a", from_account.name)("t", to_account.name)("total_transfer", db.to_pretty_string(token.amount))("balance", db.to_pretty_string(db.get_balance(from_account, asset_type))));
            /******************************** asset permission check to avoid to reduce redundant permission verification********************************************/
            if (from_account.id != asset_type.issuer && to_account.id != asset_type.issuer)
            {
                GRAPHENE_ASSERT(
                    graphene::chain::is_authorized_asset(db, from_account, asset_type),
                    transfer_from_account_not_whitelisted,
                    "'from' account ${from} is not whitelisted for asset ${asset}",
                    ("from", from)("asset", token.asset_id));
                GRAPHENE_ASSERT(
                    graphene::chain::is_authorized_asset(db, to_account, asset_type),
                    transfer_to_account_not_whitelisted,
                    "'to' account ${to} is not whitelisted for asset ${asset}",
                    ("to", to)("asset", token.asset_id));
            }
            if (asset_type.is_transfer_restricted() && (!asset_type.is_white_list()))
            {
                GRAPHENE_ASSERT(
                    from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
                    transfer_restricted_transfer_asset,
                    "Asset {asset} has transfer_restricted flag enabled",
                    ("asset", token.asset_id));
            }
        }
        FC_RETHROW_EXCEPTIONS(error, "Unable to transfer ${a} from ${f} to ${t}", ("a", db.to_pretty_string(token.amount))("f", from_account.name)("t", to_account.name));

        FC_ASSERT(token.amount >= share_type(0), "token amount must big than zero");
        db.adjust_balance(from, -token);
        db.adjust_balance(to, token);
        if (enable_logger)
        {
            graphene::chain::token_affected contract_transaction;
            contract_transaction.affected_account = from;
            contract_transaction.affected_asset = asset(-token.amount, token.asset_id);
            result.contract_affecteds.push_back(std::move(contract_transaction));
            contract_transaction.affected_account = to;
            contract_transaction.affected_asset = token;
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        wdump((e.to_string()));
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
void register_scheduler::adjust_lock_asset(string symbol_or_id, int64_t amount)
{
    try
    {
        //FC_ASSERT(caller==contract.owner, "in function:adjust_lock_asset");
        FC_ASSERT(amount != 0, "set lock asset in function:adjust_lock_asset");
        auto contract_owner = contract.owner(db);
        auto target_asset = get_asset(symbol_or_id).get_id();
        FC_ASSERT(target_asset != GRAPHENE_ASSET_GAS);
        auto contract_id = contract.get_id();
        map<asset_id_type, share_type> locked_asset = contract_owner.asset_locked.contract_lock_details[contract_id];
        if (locked_asset.size() > 0)
        {
            auto itr = locked_asset.find(target_asset);
            if (itr != locked_asset.end())
                itr->second += amount;
            else
                locked_asset[target_asset] = amount;
        }
        else
        {
            locked_asset[target_asset] = amount;
        }
        FC_ASSERT(locked_asset[target_asset] >= 0, "Setting asset lock amount must be >=0,asset:${asset},amount:${amount}", ("asset", symbol_or_id)("amount", amount));
        if (locked_asset[target_asset] == 0)
            locked_asset.erase(target_asset);
        contract_owner.asset_locked.contract_lock_details[contract_id] = locked_asset;
        if (contract_owner.asset_locked.contract_lock_details[contract_id].size() == 0)
            contract_owner.asset_locked.contract_lock_details.erase(contract_id);
        share_type locked_total = contract_owner.asset_locked.locked_total[target_asset];
        if (locked_total != 0)
            contract_owner.asset_locked.locked_total[target_asset] += amount;
        else
            contract_owner.asset_locked.locked_total[target_asset] = amount;
        db.assert_balance(contract_owner, asset(amount, target_asset));
        if (contract_owner.asset_locked.locked_total[target_asset] == 0)
            contract_owner.asset_locked.locked_total.erase(target_asset);
        db.modify(contract.owner(db), [&](account_object &ac) {
            ac.asset_locked = contract_owner.asset_locked;
        });
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

const asset_object &register_scheduler::get_asset(string symbol_or_id)
{
    try
    {
        if (auto id = db.maybe_id<asset_id_type>(symbol_or_id))
        {
            const auto &asset_by_id = db.get_index_type<asset_index>().indices().get<by_id>();
            auto itr = asset_by_id.find(*id);
            FC_ASSERT(itr != asset_by_id.end(), "not find asset: ${asset}", ("asset", symbol_or_id));
            return *itr;
        }
        else
        {
            const auto &idx = db.get_index_type<asset_index>().indices().get<by_symbol>();
            const auto itr = idx.find(symbol_or_id);
            FC_ASSERT(itr != idx.end(), "not find asset: ${asset}", ("asset", symbol_or_id));
            return *itr;
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
        std::terminate();
    }
}
int64_t register_scheduler::get_account_balance(account_id_type account, asset_id_type asset_id)
{
    try
    {
        return db.get_balance(account, asset_id).amount.value;
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
        return 0;
    }
}

void register_scheduler::transfer_from(account_id_type from, string to, double amount, string symbol_or_id, bool enable_logger)
{
    try
    {
        account_id_type account_to = get_account(to).id;
        auto &asset_ob = get_asset(symbol_or_id);
        auto token = asset(amount, asset_ob.id);
        transfer_by_contract(from, account_to, token, result, enable_logger);
    }
    catch (fc::exception e)
    {
        wdump((e.to_string()));
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

} // namespace chain
} // namespace graphene