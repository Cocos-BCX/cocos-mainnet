#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/nh_asset_object.hpp>
#include <graphene/chain/contract_function_register_scheduler.hpp>
#include "lundump.hpp"
#include "lstate.hpp"
namespace graphene
{
namespace chain
{
/*
        fc::ecc::private_key Get_random_private_key()
        {
            fc::sha256 sha_entropy1 = fc::ecc::private_key::generate().get_secret();
            fc::sha256 sha_entropy2 = fc::ecc::private_key::generate().get_secret();
            fc::bigint entropy1( sha_entropy1.data(), sha_entropy1.data_size() );
            fc::bigint entropy2( sha_entropy2.data(), sha_entropy2.data_size() );
            fc::bigint entropy(entropy1);
            entropy <<= 8*sha_entropy1.data_size();
            entropy += entropy2;
            string brain_key = "";

            for( int i=0; i<16; i++ )
            {
                fc::bigint choice = entropy % graphene::words::word_list_size;
                entropy /= graphene::words::word_list_size;
                if( i > 0 )
                    brain_key += " ";
                brain_key += graphene::words::word_list[ choice.to_int64() ];
            }
            return fc::ecc::private_key::regenerate(fc::sha256::hash(string(brain_key)));
        }
        
        void setField(lua_State *L, const char *key,const char * value)  
        {  
            lua_pushstring(L, value);  
            lua_setfield(L, -2, key);  
        } */

optional<lua_types> contract_object::get_lua_data(lua_scheduler &context, int index, bool check_fc)
{
    static uint16_t stack = 0;
    stack++;
    FC_ASSERT(context.mState);
    int type = lua_type(context.mState, index);
    switch (type)
    {
    case LUA_TSTRING:
    {
        stack--;
        return lua_string(lua_tostring(context.mState, index));
    }
    case LUA_TBOOLEAN:
    {
        lua_bool v_bool;
        v_bool.v = lua_toboolean(context.mState, index) ? true : false;
        stack--;
        return v_bool;
    }
    case LUA_TNUMBER:
    {
        stack--;
        return lua_number(lua_tonumber(context.mState, index));
    }
    case LUA_TTABLE:
    {
        lua_table v_table;
        lua_pushnil(context.mState); // nil 入栈作为初始 key
        while (0 != lua_next(context.mState, index))
        {
            auto op_key = get_lua_data(context, index + 1, check_fc);
            if (op_key)
            {
                lua_key key = *op_key;
                if (stack == 1)
                {
                    auto len = (sizeof(Key_Special) / sizeof(Key_Special[0]));
                    if ( std::find(Key_Special, Key_Special + len, key.key.get<lua_string>().v) == Key_Special + len &&
                        (LUA_TFUNCTION == lua_type(context.mState, index + 2)) == check_fc)
                    {
                        auto val = get_lua_data(context, index + 2, check_fc);
                        if (val)
                            v_table.v[key] = *val;
                    }
                }
                else
                {
                    bool skip = false;
                    if (key.key.which() == lua_key_variant::tag<lua_string>::value)
                        if (key.key.get<lua_string>().v == "__index")
                            skip = true;
                    if (!skip)
                    {
                        auto val = get_lua_data(context, index + 2, check_fc);
                        if (val)
                            v_table.v[key] = *val;
                    }
                }
            }
            lua_pop(context.mState, 1);
        }
        stack--;
        return v_table;
    }
    case LUA_TFUNCTION:
    {
        auto sfunc = lua_scheduler::Reader<FunctionSummary>::read(context.mState, index);
        if (sfunc)
        {
            stack--;
            return *sfunc;
        }
        break;
    }
    case LUA_TTHREAD:
    {
        break;
    }
    case LUA_TNIL:
    {
        break;
    }
    default: // LUA_TUSERDATA ,  LUA_TLIGHTUSERDATA
    {
        try
        {
            auto boost_lua_variant_object = lua_scheduler::readTopAndPop<lua_types::boost_variant>(context.mState, index);
            if (boost_lua_variant_object.which() != 0)
            {
                stack--;
                return boost::apply_visitor(boost_lua_variant_visitor(), boost_lua_variant_object);
            }
        }
        catch (...)
        {
            wlog("readTopAndPop error :type->${type}", ("type", type));
        }
    }
    }
    stack--;
    return {};
}

optional<lua_types> contract_object::parse_function_summary(lua_scheduler &context, int index)
{
    FC_ASSERT(context.mState);
    FC_ASSERT(lua_type(context.mState, index) == LUA_TTABLE);
    lua_table v_table;
    lua_pushnil(context.mState); // nil 入栈作为初始 key
    while (0 != lua_next(context.mState, index))
    {
        try
        {
            if (LUA_TFUNCTION == lua_type(context.mState, index + 2))
            {
                auto op_key = lua_string(lua_tostring(context.mState, index + 1));
                lua_key key = lua_types(op_key);
                auto sfunc = lua_scheduler::Reader<FunctionSummary>::read(context.mState, index + 2);
                if (sfunc)
                    v_table.v[key] = *sfunc;
            }
        }
        FC_CAPTURE_AND_RETHROW()
        lua_pop(context.mState, 1);
    }
    return v_table;
}

void contract_object::push_global_parameters(lua_scheduler &context, lua_map &global_variable_list, string tablename)
{

    static vector<lua_types::boost_variant> values;
    for (auto itr = global_variable_list.begin(); itr != global_variable_list.end(); itr++)
    {
        switch (itr->second.which())
        {
        case lua_type_num::nlua_table:
        {
            lua_key key = itr->first;
            values.push_back(key.cast_to_lua_types().to_boost_variant());
            lua_scheduler::set_emptyArray(context, tablename, values);
            LUATYPE_NAME(table)
            table_value = itr->second.get<LUATYPE_NAME(table)>();
            push_global_parameters(context, table_value.v, tablename);
            values.pop_back();
            break;
        }
        default:
        {
            lua_key key = itr->first;
            values.push_back(key.cast_to_lua_types().to_boost_variant());
            values.push_back(itr->second.to_boost_variant());
            lua_scheduler::push_key_and_value_stack(context, tablename, values);
            values.pop_back();
            values.pop_back();
            break;
        }
        }
    }
}
/*
using push_table_type = vector<std::pair<
    lua_types::boost_variant, lua_types::boost_variant>>;

push_table_type push_global_parameters(lua_map &global_variable_list)
{

    push_table_type values;
    for (auto itr = global_variable_list.begin(); itr != global_variable_list.end(); itr++)
    {
        switch (itr->second.which())
        {
        case lua_type_num::nlua_table:
        {
            lua_key key = itr->first;
            LUATYPE_NAME(table)
            table_value = itr->second.get<LUATYPE_NAME(table)>();
            break;
        }
        default:
        {
            lua_key key = itr->first;
            values.push_back(std::make_pair(key.cast_to_lua_types().to_boost_variant(), itr->second.to_boost_variant()));
            break;
        }
        }
    }
}
*/
void contract_object::push_table_parameters(lua_scheduler &context, lua_map &table_variable, string tablename)
{
    vector<lua_types::boost_variant> tvalues;
    tvalues.push_back(tablename);
    lua_scheduler::set_emptyArray(context, tvalues);
    push_global_parameters(context, table_variable, tablename);
}

void contract_object::set_process_value(vector<char> process_value)
{
    encryption_helper.decrypt(process_value,_process_value); 
    
}
vector<char> contract_object::set_result_process_value()
{
    this->result.process_value=encryption_helper.encrypt(_process_value);
    return this->result.process_value;
}

void contract_object::do_contract_function(account_id_type caller, string function_name, vector<lua_types> value_list,
                                           lua_map &account_data, database &db, const flat_set<public_key_type> &sigkeys,
                                           contract_result &apply_result)
{
    try
    {
        try
        {
            auto &baseENV = contract_bin_code_id_type(0)(db);
            auto abi_itr = contract_ABI.find(lua_types(lua_string(function_name)));
            FC_ASSERT(abi_itr != contract_ABI.end(), "${function_name} maybe a internal function", ("function_name", function_name));
            FC_ASSERT(value_list.size() >= abi_itr->second.get<lua_function>().arglist.size(),
                      "${function_name}`s parameter list is ${plist}...", ("function_name", function_name)("plist", abi_itr->second.get<lua_function>().arglist));
            contract_base_info cbi(*this, caller);
            lua_scheduler &context = db.get_luaVM();
            register_scheduler scheduler(db, caller, *this, this->mode, result, context, sigkeys, apply_result, account_data);
            context.new_sandbox(name, baseENV.lua_code_b.data(), baseENV.lua_code_b.size()); //sandbox
            context.load_script_to_sandbox(name, lua_code_b.data(), lua_code_b.size());
            context.writeVariable("current_contract", name);
            register_function(context, &scheduler, &cbi);
            context.writeVariable(name, "_G", "protected");
            context.writeVariable(name, "private_data", lua_scheduler::EmptyArray /*,account_data*/);
            context.writeVariable(name, "public_data", lua_scheduler::EmptyArray /*,contract_data*/);
            context.writeVariable(name, "read_list", "private_data", lua_scheduler::EmptyArray);
            context.writeVariable(name, "read_list", "public_data", lua_scheduler::EmptyArray);
            context.writeVariable(name, "write_list", "private_data", lua_scheduler::EmptyArray);
            context.writeVariable(name, "write_list", "public_data", lua_scheduler::EmptyArray);
            context.get_function(name, function_name);
            push_function_actual_parameters(context.mState, value_list);
            int err = lua_pcall(context.mState, value_list.size(), 0, 0);
            lua_types error_message;
            try
            {
                if (err)
                    error_message = lua_scheduler::readTopAndPop<lua_types>(context.mState, -1);
            }
            catch (...)
            {
                error_message = lua_string(" Unexpected errors ");
            }
            lua_pop(context.mState, -1);
            context.close_sandbox(name);
            if (err)
                FC_THROW("Try the contract resolution execution failure,${message}", ("message", error_message));
            if (this->result.existed_pv)
                this->set_result_process_value();
            for(auto&temp:result.contract_affecteds){
                if(temp.which()==contract_affected_type::tag<contract_result>::value)
                    result.relevant_datasize+=temp.get<contract_result>().relevant_datasize;
            }
            result.relevant_datasize+=fc::raw::pack_size(contract_data)+fc::raw::pack_size(account_data)+fc::raw::pack_size(result.contract_affecteds);
        }
        catch (VMcollapseErrorException e)
        {
            db.initialize_luaVM();
            throw e;
        }
    }
    FC_CAPTURE_AND_RETHROW()
}
void contract_object::push_function_actual_parameters(lua_State *L, vector<lua_types> &value_list)
{
    for (vector<lua_types>::iterator itr = value_list.begin(); itr != value_list.end(); itr++)
    {
        lua_scheduler::Pusher<lua_types>::push(L, *itr).release();
    }
}
static int compiling_contract_writer(lua_State *L, const void *p, size_t size, void *u)
{
    vector<char> *s = (vector<char> *)u;
    s->insert(s->end(), (const char *)p, (const char *)p + size);
    return 0;
}
lua_table contract_object::do_contract(string lua_code, lua_State *L)
{
    try
    {
        lua_scheduler context;
        if (L != nullptr && this->get_id() == contract_id_type())
        {
            context.mState = L;
            context.ismthread = false;
            compiling_contract(context.mState, lua_code, true);
            lua_setglobal(context.mState, "baseENV");
            lua_getglobal(context.mState, "baseENV");
            int err = lua_pcall(context.mState, 0, 1, 0);
            FC_ASSERT(err == 0, "Try the contract resolution compile failure,${message}", ("message", string(lua_tostring(context.mState, -1))));
            if (!lua_istable(context.mState, -1))
            {
                lua_pushnil(context.mState);
                lua_setglobal(context.mState, "baseENV");
                FC_THROW("Not a legitimate basic contract");
            }
            return lua_table();
        }
        else
        {
            compiling_contract(context.mState, lua_code);
            lua_getglobal(context.mState, name.c_str());
            auto ret = parse_function_summary(context, lua_gettop(context.mState))->get<lua_table>();
            lua_pushnil(context.mState);
            lua_setglobal(context.mState, name.c_str());
            return ret;
        }
    }
    FC_CAPTURE_AND_RETHROW()
}

void contract_object::compiling_contract(lua_State *bL, string lua_code, bool is_baseENV)
{
    if (!is_baseENV)
    {
        lua_getglobal(bL, name.c_str());
        FC_ASSERT(lua_isnil(bL, -1), "The contract already exists,name:${name}", ("name", name));
        lua_newtable(bL);
        lua_setglobal(bL, name.c_str());
    }
    int err = luaL_loadbuffer(bL, lua_code.data(), lua_code.size(), name.c_str());
    FC_ASSERT(err == 0, "Try the contract resolution compile failure,${message}", ("message", string(lua_tostring(bL, -1))));
    if (!is_baseENV)
    {
        lua_getglobal(bL, name.c_str());
        lua_setupvalue(bL, -2, 1);
    }
    Proto *f = getproto(bL->top - 1);
    lua_lock(bL);
    lua_code_b.clear();
    luaU_dump(bL, f, compiling_contract_writer, &lua_code_b, 0);
    lua_unlock(bL);
    if (!is_baseENV)
    {
        err |= lua_pcall(bL, 0, 0, 0);
        FC_ASSERT(err == 0, "Try the contract resolution compile failure,${message}", ("message", string(lua_tostring(bL, -1))));
    }
}

} // namespace chain
} // namespace graphene