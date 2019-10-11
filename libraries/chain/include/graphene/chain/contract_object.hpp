#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/protocol/lua_types.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/database.hpp>
#include <lua_extern.hpp>
#include <graphene/chain/protocol/lua_scheduler.hpp>
#include <graphene/chain/protocol/memo.hpp>
#include <graphene/utilities/words.hpp>
#include <fc/crypto/aes.hpp>
#include <graphene/process_encryption/process_encryption_helper.hpp>
namespace graphene
{
namespace chain
{
using namespace process_encryption;
struct register_scheduler;
//nico lua Key_Special
static const std::string Key_Special[] = {"table", "_G", "_VERSION", "coroutine", "debug", "math", "io",
                                          "utf8", "bit", "package", "string", "os", "cjson","baseENV"
#if !defined(LUA_COMPAT_GRAPHENE)
                                          ,
                                          "require"
#endif
};
struct contract_base_info;
class contract_object : public graphene::db::abstract_object<contract_object>
{
  public:
    static const uint8_t space_id = protocol_ids;
    static const uint8_t type_id = contract_object_type;
    time_point_sec creation_date;
    account_id_type owner;
    string name;
    bool is_release=false;
    tx_hash_type current_version;
    bool check_contract_authority = false;
    public_key_type contract_authority;
    lua_map contract_data;
    lua_map contract_ABI;
    contract_bin_code_id_type lua_code_b_id;

  public:
    contract_object(){};
    contract_object(string name):name(name){id=contract_id_type(1);}
    void compiling_contract(lua_State *bL, string lua_code,bool is_baseENV=false);
    contract_id_type get_id() const { return id; }
    optional<lua_types> parse_function_summary(lua_scheduler &context, int index);
    lua_table do_contract(string lua_code,lua_State *L=nullptr);
    void do_contract_function(account_id_type caller, string function_name, vector<lua_types> value_list,
                              lua_map &account_data, graphene::chain::database &db, const flat_set<public_key_type> &sigkeys, contract_result &apply_result);
    void set_mode(transaction_apply_mode tx_mode) { mode = tx_mode; }
    contract_result get_result() { return this->result; }
    struct process_variable &get_process_variable() { return _process_value; }
    void set_process_value(vector<char> process_value);
    vector<char> set_result_process_value();
    bool check_contract_authority_falg() { return check_contract_authority; }
    void set_process_encryption_helper(const process_encryption_helper helper) {encryption_helper=helper;  }
    void register_function(lua_scheduler &context, register_scheduler *fc_register,graphene::chain::contract_base_info*base_info)const;
    optional<lua_types> get_lua_data(lua_scheduler &context, int index, bool check_fc = false);
    void push_global_parameters(lua_scheduler &context, lua_map &global_variable_list, string tablename = "");
    void push_table_parameters(lua_scheduler &context, lua_map &table_variable, string tablename);
    void push_function_actual_parameters(lua_State *L, vector<lua_types> &value_list);
    void get_code(vector<char>&target){target=lua_code_b;lua_code_b.clear();}
    void set_code(vector<char>source){FC_ASSERT(source.size()>0); lua_code_b=source;}
  private:
    process_encryption_helper encryption_helper; 
    vector<char> lua_code_b;
    contract_result result;
    transaction_apply_mode mode;
    struct process_variable _process_value;
};

struct boost_lua_variant_visitor : public boost::static_visitor<lua_types>
{
    template <typename T>
    lua_types operator()(T t) const { return t; }
};
class account_contract_data : public graphene::db::abstract_object<account_contract_data>
{
  public:
    static const uint8_t space_id = protocol_ids;
    static const uint8_t type_id = contract_data_type;
    account_id_type owner;
    contract_id_type contract_id;
    lua_map contract_data;
    account_contract_data(){};
    contract_data_id_type get_id() const { return id; }
};

struct contract_base_info
{
    string name;
    string id;
    string owner;
    string caller;
    string creation_date;
    string contract_authority;
    contract_base_info(const contract_object &co, account_id_type caller)
    {
        this->name = co.name;
        this->id = string(co.id);
        object_id_type temp=co.owner;
        this->owner =string(temp);
        temp=caller;
        this->caller = string(temp);
        this->creation_date = string(co.creation_date);
        this->contract_authority = string(co.contract_authority);
    }
};
/*
class contract_runing_stata : public graphene::db::abstract_object<contract_runing_stata>
{
    contract_id_type contract_id;
    bool isruning;
    fc::time_point_sec last_call_time;
};
*/
#ifdef INCREASE_CONTRACT
/*********************************************合约索引�?**********************************************/
struct by_name;
struct by_owner;
typedef multi_index_container<
    contract_object,
    indexed_by<
        ordered_unique<
            tag<by_id>,
            member<object, object_id_type, &object::id>>,
        ordered_unique<
            tag<by_name>,
            member<contract_object, string, &contract_object::name>>,
        ordered_non_unique<
            tag<by_owner>,member<contract_object,account_id_type,&contract_object::owner>>
        >
    > contract_multi_index_type;

/**
    * @ingroup object_index
    */
typedef generic_index<contract_object, contract_multi_index_type> contract_index;

/*********************************************合约数据索引�?**********************************************/
struct by_account_contract
{
};
typedef multi_index_container<
    account_contract_data,
    indexed_by<
        ordered_unique<
            tag<by_id>, member<object, object_id_type, &object::id>>,
        ordered_unique<
            tag<by_account_contract>,
            composite_key<
                account_contract_data,
                member<account_contract_data, account_id_type, &account_contract_data ::owner>,
                member<account_contract_data, contract_id_type, &account_contract_data ::contract_id>>>>>
    account_contract_data_multi_index_type;

/**
    * @ingroup object_index
    */
typedef generic_index<account_contract_data, account_contract_data_multi_index_type> account_contract_data_index;


class contract_bin_code_object: public graphene::db::abstract_object<contract_bin_code_object>
{
    public:
    static const uint8_t space_id = implementation_ids;
    static const uint8_t type_id = impl_contract_bin_code_type;
    contract_id_type contract_id;
    vector<char> lua_code_b;
};
struct by_contract_id{};
typedef multi_index_container<
    contract_bin_code_object,
    indexed_by<
        ordered_unique<
            tag<by_id>, member<object, object_id_type, &object::id>>,
        ordered_unique<
            tag<by_contract_id>,member<contract_bin_code_object,contract_id_type,&contract_bin_code_object::contract_id>>
            >
        >
    contract_bin_code_multi_index_type;
typedef generic_index<contract_bin_code_object, contract_bin_code_multi_index_type> contract_bin_code_index;

#endif
} // namespace chain
} // namespace graphene

FC_REFLECT_DERIVED(graphene::chain::contract_object,
                   (graphene::db::object),
                   (creation_date)(owner)(name)(current_version)(contract_authority)(is_release)(check_contract_authority)(contract_data)(contract_ABI)(lua_code_b_id))
FC_REFLECT_DERIVED(graphene::chain::account_contract_data,
                   (graphene::db::object),
                   (owner)(contract_id)(contract_data))
FC_REFLECT_DERIVED(graphene::chain::contract_bin_code_object,(graphene::db::object),(contract_id)(lua_code_b))