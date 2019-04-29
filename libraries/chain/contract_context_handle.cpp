#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/contract_function_register_scheduler.hpp>
namespace graphene
{
namespace chain
{

//find_luaContext:按深度查找合约树形结构上下文
//context:树形结构合约相关上下文
//keys:目标路径(树形结构的某一条分支)
//start:指定查找路径的起点
//is_clean:是否清理目标
std::pair<bool, lua_types *> register_scheduler::find_luaContext(lua_map *context, vector<lua_types> keys, int start, bool is_clean)
{
    lua_types *result = nullptr;
    vector<lua_key> temp_keys;
    lua_types *temp = nullptr;
    for (unsigned int i = start; i < keys.size(); i++)
    {
        auto reslut_itr = context->find(keys[i]);
        if (reslut_itr != context->end())
        {
            temp_keys.push_back(keys[i]);
            result = &reslut_itr->second;
            if (i == keys.size() - 1)
            {
                if (is_clean)
                {
                    context->erase(reslut_itr);        //找到目标并清理目标
                    return std::make_pair(true, temp); //返回上一级树形结构节点指针，temp=nullptr时表示路径查找从start处失败
                }
                else
                    return std::make_pair(true, result); //返回目标节点指针
            }
            if (reslut_itr->second.which() == lua_types::tag<lua_table>::value)
            {
                temp = &reslut_itr->second;
                context = &reslut_itr->second.get<lua_table>().v; //深度递进
            }
            else
                return std::make_pair(false, result); //原目标不是table对象，返回上级table地址
        }
        else
            return std::make_pair(false, result); //路径查找keys[i]失败，返回上一级节点指针，result=nullptr时表示路径查找从start处失败
    }
    return std::make_pair(false, result); //start值超出keys深度，未进行查找
}

void register_scheduler::read_cache()
{
    try
    {
        auto keys = lua_scheduler::read_lua_value<lua_table>(this->context, contract.name.data(), vector<string>{"read_list"});
        for (auto &itr : keys.v)
        {
            if (itr.first.key.which() == lua_key_variant::tag<lua_string>::value && itr.first.key.get<lua_string>().v == "private_data")
            {
                vector<lua_types> stacks = {lua_string("private_data")};
                read_context(itr.second.get<lua_table>().v, this->account_conntract_data, stacks, contract.name);
            }
            if (itr.first.key.which() == lua_key_variant::tag<lua_string>::value && itr.first.key.get<lua_string>().v == "public_data")
            {
                vector<lua_types> stacks = {lua_string("public_data")};
                read_context(itr.second.get<lua_table>().v, this->contract.contract_data, stacks, contract.name);
            }
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
    catch (std::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.what());
    }
}

void register_scheduler::fllush_cache()
{
    try
    {
        auto keys = lua_scheduler::read_lua_value<lua_table>(this->context, contract.name.data(), vector<string>{"write_list"});
        for (auto &itr : keys.v)
        {
            if (itr.first.key.which() == lua_key_variant::tag<lua_string>::value && itr.first.key.get<lua_string>().v == "private_data")
            {
                vector<lua_types> stacks = {lua_string("private_data")};
                fllush_context(itr.second.get<lua_table>().v, this->account_conntract_data, stacks, contract.name);
            }
            if (itr.first.key.which() == lua_key_variant::tag<lua_string>::value && itr.first.key.get<lua_string>().v == "public_data")
            {
                vector<lua_types> stacks = {lua_string("public_data")};
                fllush_context(itr.second.get<lua_table>().v, this->contract.contract_data, stacks, contract.name);
            }
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
    catch (std::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.what());
    }
}
/******************************************************************************************************************************************************
*read_context:按指定的树型表推送对应的合约上下文
*keys:指定的需要推送的树形结构路径图 
*data_table:树形结构合约上下文数据源
*stacks:将keys树型路径图分割为单一路径的路径缓存栈
*tablename:指定VM指定的环境目标
*******************************************************************************************************************************************************/

void register_scheduler::read_context(lua_map &keys, lua_map &data_table, vector<lua_types> &stacks, string tablename)
{
    static auto start_key = lua_types(lua_string("start"));
    static auto stop_key = lua_types(lua_string("stop"));
    uint32_t start = 0, stop = 0, index = 0;
    if (keys.find(start_key) != keys.end() && keys[start_key].which() == lua_types::tag<lua_int>::value)
    {
        start = keys[start_key].get<lua_int>().v;
        keys.erase(start_key);
    }
    if (keys.find(stop_key) != keys.end() && keys[stop_key].which() == lua_types::tag<lua_int>::value)
    {
        stop = keys[stop_key].get<lua_int>().v;
        keys.erase(stop_key);
    }
    if (start || stop)
    {
        lua_map *itr_prt = nullptr;
        if (stacks.size() == 1)
            itr_prt = &data_table;
        else
        {    auto finded = find_luaContext(&data_table, stacks, 1);
            if (finded.first && finded.second->which() == lua_types::tag<lua_table>::value)
                itr_prt = &finded.second->get<lua_table>().v;
        }
        if (itr_prt)
            for (auto &itr_index : *itr_prt)
            {
                if (index >= start && stop > index)
                {
                    stacks.push_back(itr_index.first.cast_to_lua_types());
                    stacks.push_back(itr_index.second);
                    lua_scheduler::push_key_and_value_stack(context, lua_string(tablename), stacks);
                    stacks.pop_back();
                    stacks.pop_back();
                }
                if (stop <= index)
                    break;
                index++;
            }
    }
    for (auto itr = keys.begin(); itr != keys.end(); itr++)
    {
        if (itr->second.which() != lua_types::tag<lua_table>::value || itr->second.get<lua_table>().v.size() == 0)
        {
            lua_key key = itr->first;
            stacks.push_back(key.cast_to_lua_types());
            auto finded = find_luaContext(&data_table, stacks, 1);
            if (finded.first)
            {
                stacks.push_back(*finded.second);
                lua_scheduler::push_key_and_value_stack(context, lua_string(tablename), stacks);
                stacks.pop_back();
            }
            stacks.pop_back();
        }
        else if (itr->second.which() == lua_types::tag<lua_table>::value && itr->second.get<lua_table>().v.size() > 0)
        {
            stacks.push_back(itr->first.cast_to_lua_types());
            read_context(itr->second.get<lua_table>().v, data_table, stacks, tablename);
            stacks.pop_back();
        }
    }
    if (keys.size() == 0 && stacks.size() == 1 && !(start || stop))
    {
        lua_scheduler::push_key_and_value_stack(context, lua_string(tablename), vector<lua_types>{stacks[0], lua_table(data_table)});
    }
}
/******************************************************************************************************************************************************
*fllush_context:按指定的树型表更新链上对应的合约上下文
*keys:指定的需要推送的树形结构路径图 
*data_table:树形结构合约上下文数据源
*stacks:将keys树型路径图分割为单一路径的路径缓存栈
*tablename:指定VM指定的环境目标
*******************************************************************************************************************************************************/
void register_scheduler::fllush_context(const lua_map &keys, lua_map &data_table, vector<lua_types> &stacks, string table_name)
{
    for (auto itr = keys.begin(); itr != keys.end(); itr++)
    {
        if (itr->second.which() != lua_types::tag<lua_table>::value || itr->second.get<lua_table>().v.size() == 0)
        {
            lua_key key = itr->first;
            bool flag = (itr->second.which() == lua_types::tag<lua_bool>::value) ? itr->second.get<lua_bool>().v : true; //默认不清理对应的上下文
            stacks.push_back(key.cast_to_lua_types());
            if (!flag)
            {
                find_luaContext(&data_table, stacks, 1, true);
            }
            else
            {
                auto finded = find_luaContext(&data_table, stacks, 1);
                if (finded.first)
                {
                    *finded.second = lua_scheduler::read_lua_value<lua_types>(context, table_name.data(), stacks);
                }
                else if (finded.second)
                {
                    lua_table &temp_table = finded.second->get<lua_table>();
                    temp_table.v[key] = lua_scheduler::read_lua_value<lua_types>(context, table_name.data(), stacks);
                }
                else
                {
                    data_table[key] = lua_scheduler::read_lua_value<lua_types>(context, table_name.data(), vector<lua_types>{stacks[0], stacks[1]});
                }
            }
            stacks.pop_back();
        }
        else if (itr->second.which() == lua_types::tag<lua_table>::value && itr->second.get<lua_table>().v.size() > 0)
        {
            lua_key key = itr->first;
            stacks.push_back(key.cast_to_lua_types());
            auto finded = find_luaContext(&data_table, stacks, 1);
            if (finded.first)
            {
                fllush_context(itr->second.get<lua_table>().v, data_table, stacks, table_name);
            }
            else if (finded.second)
            {
                lua_table &temp_table = finded.second->get<lua_table>();
                temp_table.v[key] = lua_scheduler::read_lua_value<lua_types>(context, table_name.data(), stacks);
            }
            else
            {
                data_table[key] = lua_scheduler::read_lua_value<lua_types>(context, table_name.data(), vector<lua_types>{stacks[0], stacks[1]});
            }
            stacks.pop_back();
        }
    }
    if (keys.size() == 0)
    {
        data_table = lua_scheduler::read_lua_value<lua_table>(context, table_name.data(), vector<lua_types>{stacks[0]}).v;
    }
}
/******************************************************************************************************************************************************
*filter_context:按指定的树型表筛选对应的合约上下文
*data_table:树形结构合约上下文数据源
*keys:指定的需要推送的树形结构路径图 
*stacks:将keys树型路径图分割为单一路径的路径缓存栈
*result:存储筛选结果
*******************************************************************************************************************************************************/
void register_scheduler::filter_context(const lua_map &data_table,  lua_map keys, vector<lua_types> &stacks, lua_map *result)
{
    static auto start_key = lua_types(lua_string("start"));
    static auto stop_key = lua_types(lua_string("stop"));
    uint32_t start = 0, stop = 0, index = 0;
    if (keys.find(start_key) != keys.end() && keys[start_key].which() == lua_types::tag<lua_int>::value)
    {
        start = keys[start_key].get<lua_int>().v;
        keys.erase(start_key);
    }
    if (keys.find(stop_key) != keys.end() && keys[stop_key].which() == lua_types::tag<lua_int>::value)
    {
        stop = keys[stop_key].get<lua_int>().v;
        keys.erase(stop_key);
    }
    if (start || stop)
    {
       const lua_map *itr_prt = nullptr;
        if (stacks.size() == 0)
            itr_prt = &data_table;
        else
        {    auto finded = find_luaContext(const_cast< lua_map* >(&data_table), stacks);
            if (finded.first && finded.second->which() == lua_types::tag<lua_table>::value)
                itr_prt = &finded.second->get<lua_table>().v;
        }
        if (itr_prt)
            for (auto &itr_index : *itr_prt)
            {
                if (index >= start && stop > index)
                    (*result)[itr_index.first] = itr_index.second;
                if (stop <= index)
                    break;
                index++;
            }
    }
    for (auto itr = keys.begin(); itr != keys.end(); itr++)
    {
        if (itr->second.which() != lua_types::tag<lua_table>::value || itr->second.get<lua_table>().v.size() == 0)
        {
            lua_key key = itr->first;
            stacks.push_back(key.cast_to_lua_types());
            auto finded = find_luaContext(const_cast<lua_map *>(&data_table), stacks);
            if (finded.first)
                (*result)[key] = *finded.second;
            stacks.pop_back();
        }
        else if (itr->second.which() == lua_types::tag<lua_table>::value && itr->second.get<lua_table>().v.size() > 0)
        {
            lua_key key = itr->first;
            stacks.push_back(key.cast_to_lua_types());
            (*result)[key] = lua_table();
            filter_context(data_table, itr->second.get<lua_table>().v, stacks, &(*result)[key].get<lua_table>().v);
            if ((*result)[key].get<lua_table>().v.size() == 0)
                result->erase(key);
            stacks.pop_back();
        }
    }
}

} // namespace chain
} // namespace graphene