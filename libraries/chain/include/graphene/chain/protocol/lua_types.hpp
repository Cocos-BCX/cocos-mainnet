
#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene
{
namespace chain
{
#define LUATYPE(T)                       \
    typedef struct lua_##T               \
    {                                    \
        T v;                     \
        lua_##T(T v)             \
        {                                \
            this->v = v; \
        }                                \
        lua_##T() {}                     \
    } lua_##T;
#define LUATYPE_NAME(T) lua_##T

//BOOST_PP_SEQ_FOR_EACH 是一个重复展开的宏 ，用法如下：
/*
语法：
    BOOST_PP_SEQ_FOR_EACH(macro, data, seq)  
变量：
macro:一个以格式macro(r, data, elem)定义的三元宏。
该宏被BOOST_PP_SEQ_FOR_EACH按照seq中每个元素进行展开。展开该宏，需要用到下一个BOOST_PP_FOR的重复项, 备用数据data和当前元素。
data
备用数据，用于传给macro
seq
用于供macro按照哪个序列进行展开,如果序列是(a)(b)(c)，则展开为：
macro(r, data, a) macro(r, data, b) macro(r, data, c)
如果要求更高效率，可使用BOOST_PP_SEQ_FOR_EACH_R
*/

struct lua_table;
struct lua_key;
struct memo_data;

#define SET_LUA_BASE_TYPE(r, data, T) \
    LUATYPE(T)
BOOST_PP_SEQ_FOR_EACH(SET_LUA_BASE_TYPE, UNUSED, (bool)(int64_t)(double)(string))

/*
typedef  struct lua_string {                   
        std::string v;                                 
        lua_string (std::string v)                      
        {                                       
            this->v=v;      
        }                                      
        lua_string(){}                           
}lua_string;
*/
typedef LUATYPE_NAME(double) lua_number;
typedef LUATYPE_NAME(int64_t) lua_int;

enum lua_type_num
{
    nlua_int = 0,
    nlua_number = 1,
    nlua_string = 2,
    nlua_bool = 3,
    nlua_table = 4
};

#define lua_userdata memo_data, \
                     asset /*6*/

struct FunctionSummary
{ //处理lua函数信息
    //short arg_num;  //入参个数
    //short ret_num;  //返回值个数
    bool is_var_arg;                  //是否为可变参数
    std::vector<std::string> arglist; //入参参数名，按序push
    //std::vector<std::string> retlist; //返回参数名，按序push
};

typedef struct FunctionSummary lua_function;

typedef static_variant<
    LUATYPE_NAME(int),    //0
    LUATYPE_NAME(number), //1
    LUATYPE_NAME(string), //2
    LUATYPE_NAME(bool),   //3
    LUATYPE_NAME(table),  //4
    LUATYPE_NAME(function),
    lua_userdata>
    lua_types;
typedef static_variant<
    LUATYPE_NAME(int),
    LUATYPE_NAME(number),
    LUATYPE_NAME(string),
    LUATYPE_NAME(bool)>
    lua_key_variant;

typedef std::map<lua_key, lua_types> lua_map;
typedef struct lua_table
{
    lua_map v;
    lua_table(lua_map v)
    {
        this->v = v;
    }
    lua_table(){};
} lua_table;

typedef struct lua_key
{
    lua_key_variant key;
    lua_key(){};
    lua_key(lua_types lkey)
    {
        key = cast_from_lua_types(lkey);
    }

    static lua_key_variant cast_from_lua_types(lua_types lt)
    {
        switch (lt.which())
        {
        case 0:
        {
            return lt.get<LUATYPE_NAME(int)>();
        }
        case 1:
        {
            return lt.get<LUATYPE_NAME(number)>();
        }
        case 2:
        {
            return lt.get<LUATYPE_NAME(string)>();
        }
        default:
            FC_THROW("lua_key::cast_from_lua_types error ,lua_types:${lua_types}", ("lua_types", lt)); //return LUATYPE_NAME(bool)(false);
        }
    }
    lua_types cast_to_lua_types()const
    {
        switch (key.which())
        {
        case 0:
        {
            return key.get<LUATYPE_NAME(int)>();
        }
        case 1:
        {
            return key.get<LUATYPE_NAME(number)>();
        }
        case 2:
        {
            return key.get<LUATYPE_NAME(string)>();
        }
        default:
            return lua_types();
        }
    }
    lua_key &operator=(const lua_key &v)
    {
        if (this == &v)
            return *this;
        this->key = v.key;
        return *this;
    }
    lua_key &operator=(const lua_types &lt)
    {
        key = cast_from_lua_types(lt);
        return *this;
    }

    friend bool operator==(const lua_key &a, const lua_key &b)
    {
        if (a.key.which() == b.key.which())
        {
            switch (a.key.which())
            {
            case 0:
            {
                LUATYPE_NAME(int)
                ad = a.key.get<LUATYPE_NAME(int)>();
                LUATYPE_NAME(int)
                bd = b.key.get<LUATYPE_NAME(int)>();
                return ad.v == bd.v;
            }
            case 1:
            {
                LUATYPE_NAME(number)
                ad = a.key.get<LUATYPE_NAME(number)>();
                LUATYPE_NAME(number)
                bd = b.key.get<LUATYPE_NAME(number)>();
                return ad.v == bd.v;
            }
            case 2:
            {
                LUATYPE_NAME(string)
                ad = a.key.get<LUATYPE_NAME(string)>();
                LUATYPE_NAME(string)
                bd = b.key.get<LUATYPE_NAME(string)>();
                return ad.v == bd.v;
            }
            default:
                return false;
            }
        }
        return false;
    }
    friend bool operator<(const lua_key &a, const lua_key &b)
    {
        if (a.key.which() == b.key.which())
        {
            switch (a.key.which())
            {
            case 0:
            {
                LUATYPE_NAME(int)
                ad = a.key.get<LUATYPE_NAME(int)>();
                LUATYPE_NAME(int)
                bd = b.key.get<LUATYPE_NAME(int)>();
                return ad.v < bd.v;
            }
            case 1:
            {
                LUATYPE_NAME(number)
                ad = a.key.get<LUATYPE_NAME(number)>();
                LUATYPE_NAME(number)
                bd = b.key.get<LUATYPE_NAME(number)>();
                return ad.v < bd.v;
            }
            case 2:
            {
                LUATYPE_NAME(string)
                ad = a.key.get<LUATYPE_NAME(string)>();
                LUATYPE_NAME(string)
                bd = b.key.get<LUATYPE_NAME(string)>();
                return ad.v < bd.v;
            }
            default:
                return false;
            }
        }
        return a.key.which() < b.key.which();
    }

} lua_key;

} // namespace chain
} // namespace graphene
FC_REFLECT_TYPENAME(graphene::chain::lua_types)
FC_REFLECT_TYPENAME(graphene::chain::lua_key_variant)
FC_REFLECT(graphene::chain::FunctionSummary, //(arg_num)(ret_num)
           (is_var_arg)(arglist)              //(retlist)
)
FC_REFLECT(graphene::chain::LUATYPE_NAME(key), (key))
FC_REFLECT(graphene::chain::LUATYPE_NAME(bool), (v))
FC_REFLECT(graphene::chain::LUATYPE_NAME(int64_t), (v))
FC_REFLECT(graphene::chain::LUATYPE_NAME(string), (v))
FC_REFLECT(graphene::chain::LUATYPE_NAME(double), (v))
FC_REFLECT(graphene::chain::LUATYPE_NAME(table), (v))
