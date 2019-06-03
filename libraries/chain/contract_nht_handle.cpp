#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/nh_asset_object.hpp>
#include <graphene/chain/nh_asset_creator_object.hpp>
#include <graphene/chain/world_view_object.hpp>
#include <graphene/chain/contract_function_register_scheduler.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
namespace graphene
{
namespace chain
{

const nh_asset_object &register_scheduler::get_nh_asset(string hash_or_id)
{
    try
    {
        if (auto id = db.maybe_id<nh_asset_id_type>(hash_or_id))
        {
            const auto &nh_assets = db.get_index_type<nh_asset_index>().indices().get<by_id>();
            auto itr = nh_assets.find(*id);
            FC_ASSERT(itr != nh_assets.end(), "not find non homogeneity token: ${token}", ("token", hash_or_id));
            return *itr;
        }
        else
        {
            const auto &nh_assets = db.get_index_type<nh_asset_index>().indices().get<by_nh_asset_hash_id>();
            auto itr = nh_assets.find(nh_hash_type(hash_or_id));
            FC_ASSERT(itr != nh_assets.end(), "not find non homogeneity token: ${token}", ("token", hash_or_id));
            return *itr;
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
} // namespace chain
string register_scheduler::create_nh_asset(string owner_id_or_name, string symbol, string world_view, string base_describe, bool enable_logger)
{
    auto owner_id = get_account(owner_id_or_name).get_id();
    try
    {
        const auto &nh_asset_creator_idx_by_nh_asset_creator = db.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();
        const auto &nh_asset_creator_idx = nh_asset_creator_idx_by_nh_asset_creator.find(contract.owner);
        FC_ASSERT(nh_asset_creator_idx != nh_asset_creator_idx_by_nh_asset_creator.end(), "contract owner isn't a nh asset creator, so you can't create nh asset.");
        FC_ASSERT(find(nh_asset_creator_idx->world_view.begin(), nh_asset_creator_idx->world_view.end(), world_view) != nh_asset_creator_idx->world_view.end(),
                  "contract owner don't have this world view.");
        // Verify that if the asset exists
        const auto &asset_idx_by_symbol = db.get_index_type<asset_index>().indices().get<by_symbol>();
        const auto &asset_idx = asset_idx_by_symbol.find(symbol);
        FC_ASSERT(asset_idx != asset_idx_by_symbol.end(), "The asset id is not exist.");
        // Verify that if the world view exists
        const auto &version_idx_by_symbol = db.get_index_type<world_view_index>().indices().get<by_world_view>();
        const auto &ver_idx = version_idx_by_symbol.find(world_view);
        FC_ASSERT(ver_idx != version_idx_by_symbol.end(), "The world view is not exist.");

        const nh_asset_object &nh_asset_obj = db.create<nh_asset_object>([&](nh_asset_object &nh_asset) {
            nh_asset.nh_asset_owner = owner_id;
            nh_asset.nh_asset_creator = contract.owner;
            nh_asset.nh_asset_active = owner_id;
            nh_asset.dealership = owner_id;
            nh_asset.asset_qualifier = symbol;
            nh_asset.world_view = world_view;
            nh_asset.base_describe = base_describe;
            nh_asset.create_time = db.head_block_time();
            nh_asset.get_hash();
        });
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = owner_id;
            contract_transaction.affected_item = nh_asset_obj.id;
            contract_transaction.action = nht_affected_type::create_for;
            result.contract_affecteds.push_back(std::move(contract_transaction));
            contract_transaction.affected_account = contract.owner;
            contract_transaction.action = nht_affected_type::create_by;
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
        return string(nh_asset_obj.id);
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
void register_scheduler::nht_describe_change(string nht_hash_or_id, string key, string value, bool enable_logger)
{

    try
    {
        auto &ob = get_nh_asset(nht_hash_or_id);
        FC_ASSERT(caller == ob.nh_asset_owner || caller == ob.nh_asset_active, "No permission to modify the corresponding description");
        if (ob.nh_asset_owner != ob.nh_asset_active)
        {
            if (caller == ob.nh_asset_active)
            {
                auto limit_itr = find(ob.limit_list.begin(), ob.limit_list.end(), contract.id);
                if (ob.limit_type == nh_asset_lease_limit_type::white_list)
                    FC_ASSERT(limit_itr != ob.limit_list.end(), "The designated contracts not found on the whitelist,contract:${contract}", ("contract", contract.id));
                else
                    FC_ASSERT(limit_itr == ob.limit_list.end(), "The designated contract was found on the blacklist,contract:${contract}", ("contract", contract.id));
            }
            else
                FC_THROW("The designated contract was found on the blacklist,nht_token:${nht_token}", ("nht_token", ob.id));
        }
        if (key.empty())
            return;
        auto contract_nht_describe_itr = ob.describe_with_contract.find(contract.id);
        if (contract_nht_describe_itr == ob.describe_with_contract.end())
        {
            if (value.empty())
                return;
            map<string, string> temp_describe;
            temp_describe[key] = value;
            db.modify(ob, [&](nh_asset_object &gob) {
                gob.describe_with_contract[contract.id] = temp_describe;
            });
        }
        else
        {
            optional<map<string, string>> contract_nht_describe = contract_nht_describe_itr->second;
            auto find_itr = contract_nht_describe->find(key);
            if (find_itr != (*contract_nht_describe).end())
            {
                if (value.empty())
                    contract_nht_describe->erase(find_itr);
                else
                    find_itr->second = value;
            }
            else
            {
                if (value.empty())
                    return;
                (*contract_nht_describe).insert(map<string, string>::value_type(key, value));
            }
            db.modify(ob, [&](nh_asset_object &gob) {
                if (contract_nht_describe->size() == 0)
                    gob.describe_with_contract.erase(gob.describe_with_contract.find(contract.id));
                else
                    gob.describe_with_contract[contract.id] = *contract_nht_describe;
            });
        }
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = caller;
            contract_transaction.affected_item = ob.id;
            contract_transaction.action = nht_affected_type::modified;
            contract_transaction.modified = std::pair<string, string>(key, value);
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

void register_scheduler::transfer_nht(account_id_type from, account_id_type account_to, const nh_asset_object &token, bool enable_logger)
{
    try
    {
        FC_ASSERT(token.nh_asset_owner == from, "You'e not the nh asset's owner, so you can't transfer it,nh asset:${token}.", ("token", token)); //校验交易人是否为道具所有人
        FC_ASSERT(token.nh_asset_active == from, "You don`t have the nh asset's active, so you can't transfer it,nh asset:${token}.", ("token", token));
        FC_ASSERT(token.dealership == from, "You don`t have the nh asset's dealership, so you can't transfer it,nh asset:${token}.", ("token", token));
        FC_ASSERT(account_to != from, "You can't transfer it to yourslef,nh asset:${token}.", ("token", token));
        db.modify(token, [&](nh_asset_object &g) {
            g.nh_asset_owner = account_to;
            g.nh_asset_active = account_to;
            g.dealership = account_to;
        });
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = from;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_from;
            result.contract_affecteds.push_back(std::move(contract_transaction));
            contract_transaction.affected_account = account_to;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_to;
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

// transfer of non homogeneous asset's use rights
void register_scheduler::transfer_nht_active(account_id_type from, account_id_type account_to, const nh_asset_object &token, bool enable_logger)
{
    try
    {
        if (token.dealership != from) // if the trader is not the authority account, carry out the following checks
        {
            FC_ASSERT(token.nh_asset_active == from, "You're not the nh asset's active, so you can't transfer it'suse rights, nh asset:${token}.", ("token", token));
        }

        FC_ASSERT(account_to != token.nh_asset_active, "You can't transfer it to yourslef, nh asset:${token}.", ("token", token));
        db.modify(token, [&](nh_asset_object &g) {from= g.nh_asset_active;g.nh_asset_active = account_to; });
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = from;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_active_from;
            result.contract_affecteds.push_back(std::move(contract_transaction));
            contract_transaction.affected_account = account_to;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_active_to;
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
/*
// transfer of non homogeneous asset's ownership
void register_scheduler::transfer_nht_ownership(account_id_type from, account_id_type account_to, const nh_asset_object &token, bool enable_logger)
{
    try
    {
        if (token.dealership != from) // if the trader is not the authority account, carry out the following checks
        {
            // Verify that the trader is the owner of nh asset
            FC_ASSERT(token.nh_asset_owner == from, "You're not the nh asset's owner, so you can't transfer it's ownership, nh asset:${token}.", ("token", token));
            // If the trader owns the active of the nh asset , or the ownership of the nh asset is transferred to the owner of the nh asset, the transfer of the ownership can be used.
            FC_ASSERT(token.nh_asset_active == from || token.nh_asset_active == account_to, "You're not the nh asset's active, or transfer to the active, nh asset:${token}.", ("token", token));
        }
        FC_ASSERT(account_to != token.nh_asset_owner, "You can't transfer it to yourslef, nh asset:${token}.", ("token", token));
        db.modify(token, [&](nh_asset_object &g) { g.nh_asset_owner = account_to; });
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = from;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_owner_from;
            result.contract_affecteds.push_back(std::move(contract_transaction));
            contract_transaction.affected_account = account_to;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_owner_to;
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
*/
// transfer of non homogeneous asset's authority
void register_scheduler::transfer_nht_dealership(account_id_type from, account_id_type account_to, const nh_asset_object &token, bool enable_logger)
{
    try
    {
        // Verify that the trader is the authority account of nh asset
        FC_ASSERT(token.dealership == from, "You're not the nh asset's authority account, so you can't transfer it's authority, nh asset:${token}.", ("token", token));

        FC_ASSERT(account_to != from, "You can't transfer it to yourslef, nh asset:${token}.", ("token", token));
        db.modify(token, [&](nh_asset_object &g) { g.dealership = account_to; });
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = from;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_authority_from;
            result.contract_affecteds.push_back(std::move(contract_transaction));
            contract_transaction.affected_account = account_to;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::transfer_authority_to;
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

// set non homogeneous asset's limit list
void register_scheduler::set_nht_limit_list(account_id_type nht_owner, const nh_asset_object &token, const string &contract_name_or_ids, bool limit_type, bool enable_logger)
{
    try
    {
        // Verify that the trader is the owner of nh asset and the nht must not be leasing.
        FC_ASSERT(token.nh_asset_owner == nht_owner && token.nh_asset_owner == token.nh_asset_active, "You must be the nh asset's owner, and the nht must not be leasing, nh asset:${token}.", ("token", token));
        auto temp_object = token;
        limit_type ? temp_object.limit_type = nh_asset_lease_limit_type::white_list : temp_object.limit_type = nh_asset_lease_limit_type::black_list;
        if (temp_object.limit_type != token.limit_type)
            temp_object.limit_list.clear();
        contract_id_type contract_id;
        flat_set<string> contracts;
        boost::split(contracts, contract_name_or_ids, boost::is_any_of(" ,"), boost::token_compress_on);
        for (auto contract_itr = contracts.begin(); contract_itr != contracts.end(); contract_itr++)
        {
            if (contract_itr->empty())
                continue;
            if (auto id = db.maybe_id<contract_id_type>(*contract_itr))
            {
                const auto &contracts = db.get_index_type<contract_index>().indices().get<by_id>();
                auto itr = contracts.find(*id);
                FC_ASSERT(itr != contracts.end(), "not find contract: ${contract}", ("contract", *contract_itr));
                contract_id = itr->id;
            }
            else
            {
                const auto &contracts = db.get_index_type<contract_index>().indices().get<by_name>();
                auto itr = contracts.find(*contract_itr);
                FC_ASSERT(itr != contracts.end(), "not find contract: ${contract}", ("contract", *contract_itr));
                contract_id = itr->id;
            }
            if (temp_object.limit_list.end() == find(temp_object.limit_list.begin(), temp_object.limit_list.end(), contract_id))
            {
                temp_object.limit_list.push_back(std::move(contract_id));
            }
        }
        db.modify(token, [&](nh_asset_object &g) { g = temp_object; });
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = nht_owner;
            contract_transaction.affected_item = token.id;
            contract_transaction.action = nht_affected_type::set_limit_list;
            result.contract_affecteds.push_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

void register_scheduler::relate_nh_asset(account_id_type nht_creator, const nh_asset_object &parent_nh_asset, const nh_asset_object &child_nh_asset, bool relate, bool enable_logger)
{
    try
    {
        FC_ASSERT(sigkeys.find(contract.contract_authority)!=sigkeys.end());
        FC_ASSERT(parent_nh_asset.nh_asset_owner==nht_creator,"You're not the parent nh asset's creator, so you can't relate it.${id}",("id",parent_nh_asset.id));
        FC_ASSERT(child_nh_asset.nh_asset_owner==nht_creator,"You're not the child nh asset's creator, so you can't relate it.${id}",("id",child_nh_asset.id));
        // Verify that the trader is the creator of nh asset
        const auto& contract_id = this->contract.id;
        
        const auto &iter = parent_nh_asset.child.find(contract_id);
        if ( relate )
        {
            if (iter != parent_nh_asset.child.end())
            {
                FC_ASSERT(find(iter->second.begin(), iter->second.end(), child_nh_asset.id) == iter->second.end(), "The parent item and child item had be related.");
                db.modify(parent_nh_asset, [&](nh_asset_object &g) {g.child[contract_id].emplace_back(child_nh_asset.id);});
                db.modify(child_nh_asset, [&](nh_asset_object &g) {g.parent[contract_id].emplace_back(parent_nh_asset.id);});
            }
            else
            {
                db.modify(parent_nh_asset, [&](nh_asset_object &g) {g.child.emplace(contract_id, vector<nh_asset_id_type>(1, child_nh_asset.id));});
                db.modify(child_nh_asset, [&](nh_asset_object &g) {g.parent.emplace(contract_id, vector<nh_asset_id_type>(1, parent_nh_asset.id));});
            }
        }
        else
        {
            FC_ASSERT( iter != parent_nh_asset.child.end(), "The parent nh asset's parent dosen't contain this contract.");
            FC_ASSERT(find(iter->second.begin(), iter->second.end(), child_nh_asset.id) != iter->second.end(), 
                    "The parent nh asset and child nh asset did not relate.");
            db.modify(parent_nh_asset, [&](nh_asset_object &g) {
                g.child[contract_id].erase(find(g.child[contract_id].begin(), g.child[contract_id].end(), child_nh_asset.id));
            });
            db.modify(child_nh_asset, [&](nh_asset_object &g) {
                g.parent[contract_id].erase(find(g.parent[contract_id].begin(), g.parent[contract_id].end(), parent_nh_asset.id));
            });
        }
        if (enable_logger)
        {
            graphene::chain::nht_affected contract_transaction;
            contract_transaction.affected_account = nht_creator;
            contract_transaction.affected_item = parent_nh_asset.id;
            contract_transaction.action = nht_affected_type::relate_nh_asset;
            result.contract_affecteds.emplace_back(contract_transaction);
            contract_transaction.affected_item = child_nh_asset.id;
            result.contract_affecteds.emplace_back(std::move(contract_transaction));
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
} // namespace chain
} // namespace graphene
