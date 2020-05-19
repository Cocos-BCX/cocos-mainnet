#pragma once
#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/nh_asset_object.hpp>
namespace graphene
{
namespace chain
{
#define LUA_C_ERR_THROW(L,eMsg) \
lua_scheduler::Pusher<string>::push(L, eMsg).release();\
lua_scheduler::luaError(L);

struct register_scheduler
{
    database &db;
    contract_object &contract;
    account_id_type caller;
    contract_result &result;
    struct process_variable& _process_value;
    const transaction_evaluation_state * trx_state;
    lua_scheduler &context;
    const flat_set<public_key_type>& sigkeys;
    contract_result& apply_result;
    map<lua_key,lua_types>& account_conntract_data;
    register_scheduler(database &db,account_id_type caller ,contract_object &contract,const transaction_evaluation_state * mode, 
        contract_result &result,lua_scheduler &context,const flat_set<public_key_type>& sigkeys, contract_result& apply_result,map<lua_key,lua_types>& account_data)
        : db(db),contract(contract),caller(caller), result(result),_process_value(contract.get_process_variable()),trx_state(mode),context(context),sigkeys(sigkeys),
        apply_result(apply_result),account_conntract_data(account_data){
          result.contract_id= contract.id;
        }
    bool is_owner();
    void log(string message);
    int contract_random();
    void set_permissions_flag(bool flag);
    void set_invoke_share_percent(double percent);
    void read_cache();
    void fllush_cache();
    lua_Number nummin();
    lua_Number nummax();
    int64_t integermax();
    int64_t integermin();
    const nh_asset_object& get_nh_asset(string hash_or_id);
    string hash256(string source);
    string hash512(string source);
    uint32_t head_block_time();
    uint64_t real_time();
    const account_object& get_account(string name_or_id);
    void adjust_lock_asset(string symbol_or_id,int64_t amount);
    static std::pair<bool, lua_types *>  find_luaContext( lua_map* context, vector<lua_types> keys,int start=0,bool is_clean=false);
    string create_nh_asset(string owner_id_or_name,string symbol,string world_view,string base_describe,bool enable_logger);
    void fllush_context(const lua_map& keys, lua_map &data_table,vector<lua_types>&stacks, string tablename);
    void read_context( lua_map& keys, lua_map &data_table,vector<lua_types>&stacks, string tablename);
    static void filter_context(const lua_map &data_table, lua_map keys,vector<lua_types>&stacks,lua_map *result_table);
    void transfer_by_contract(account_id_type from, account_id_type to, asset token, contract_result &result,bool enable_logger=false);
    const asset_object& get_asset(string symbol_or_id);
    int64_t get_account_balance(account_id_type account,asset_id_type asset_id);
    void transfer_from(account_id_type from, string to, double amount, string symbol_or_id,bool enable_logger=false);
    void transfer_nht(account_id_type from,account_id_type account_to,const nh_asset_object& token ,bool enable_logger=false);
    void nht_describe_change(string nht_hash_or_id,string key,string value,bool enable_logger=false);
    void change_contract_authority(string authority);
    memo_data make_memo(string receiver_id_or_name, string key, string value, uint64_t ss,bool enable_logger=false);
    void invoke_contract_function(string contract_id_or_name,string function_name,string value_list_json);
    const contract_object& get_contract(string name_or_id);
    void make_release();
	// transfer of non homogeneous asset's use rights
	void transfer_nht_active(account_id_type from,account_id_type account_to,const nh_asset_object& token ,bool enable_logger=false);
	// transfer of non homogeneous asset's ownership
    void transfer_nht_ownership(account_id_type from, account_id_type account_to,const nh_asset_object &token, bool enable_logger=false);
	// transfer of non homogeneous asset's authority
	void transfer_nht_dealership(account_id_type from, account_id_type account_to,const nh_asset_object &token, bool enable_logger=false);
	// set non homogeneous asset's limit list
	void set_nht_limit_list(account_id_type nht_owner,const nh_asset_object &token, const string& contract_name_or_ids, bool limit_type, bool enable_logger=false);
    // relate parent nh asset and child nh asset
    void relate_nh_asset(account_id_type nht_creator, const nh_asset_object &parent_nh_asset, const nh_asset_object &child_nh_asset, bool relate, bool enable_logger=false);

    // NFT relative methods
    string create_nft_asset(account_id_type owner_id, account_id_type dealer_id, string world_view, string base_describe, bool enable_logger);
    void adjust_lock_nft_asset(const nh_asset_object &token, bool lock_or_unlock=true);
    void transfer_nft_ownership(account_id_type from, account_id_type account_to, const nh_asset_object &token, bool enable_logger=false);

    void update_collateral_for_gas(string to, int64_t amount);
    lua_map get_contract_public_data(string name_or_id);
};
}}
