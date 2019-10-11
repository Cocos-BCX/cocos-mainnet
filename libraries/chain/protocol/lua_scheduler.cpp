#include <graphene/chain/protocol/lua_scheduler.hpp>
#include "lobject.hpp"
#include "lstate.hpp"
namespace graphene
{
namespace chain
{
bool lua_scheduler::new_sandbox(string spacename, const char *condition, size_t condition_size)
{
	/*lua_getglobal(mState, "_G"); //为保证栈顶顺序并正确赋值，这里强制以_G为参考坐标
	lua_getfield(mState, -1, "baseENV");
	if (lua_isnil(mState,-1))
	{
		lua_pop(mState, 1);
		luaL_loadbuffer(mState, condition, condition_size, spacename.data());
		//lua_setglobal(mState,"baseENV");
		//lua_getglobal(mState, "baseENV");
		lua_setfield(mState, -2,"baseENV");
		lua_getglobal(mState, "_G");
		lua_getfield(mState,-1,"baseENV");
	}
	int err = lua_pcall(mState, 0, 1, 0);
	if (err)
		FC_THROW("Contract loading infrastructure failure,${message}", ("message", string(lua_tostring(mState, -1))));
	lua_setfield(mState, -3, spacename.data());
	lua_pop(mState, -1);
	return true;
*/
	lua_getglobal(mState, "baseENV");
	if (lua_isnil(mState,-1))
	{
		lua_pop(mState, 1);
		luaL_loadbuffer(mState, condition, condition_size,(spacename+" baseENV").data());
		lua_setglobal(mState,"baseENV");
		lua_getglobal(mState, "baseENV");
	}
	int err = lua_pcall(mState, 0, 1, 0);
	if (err)
		FC_THROW("Contract loading infrastructure failure,${message}", ("message", string(lua_tostring(mState, -1))));
	if(!spacename.empty())
	{	
		lua_setglobal(mState, spacename.data());
		lua_pop(mState, -1);
	}
	return true;
}

bool lua_scheduler::get_sandbox(string spacename)
{
	lua_getglobal(mState, spacename.data());
	if (lua_istable(mState, -1))
		return true;
	lua_pop(mState, 1);
	return false;
}

bool lua_scheduler::close_sandbox(string spacename)
{
	lua_getglobal(mState, "_G"); /* table for ns list */
	lua_getfield(mState, -1, spacename.data());
	if (!lua_istable(mState, -1)) /* no such ns */
		return false;
	lua_pop(mState, 1);
	lua_pushnil(mState);
	lua_setfield(mState, -2, spacename.data());
	lua_gc(mState, LUA_GCCOLLECT, 0);
	return true;
}

bool lua_scheduler::load_script_to_sandbox(string spacename, const char *script, size_t script_size)
{
	int sta = luaL_loadbuffer(mState, script, script_size, spacename.data()); //  lua加载脚本之后会返回一个函数(即此时栈顶的chunk块)，lua_pcall将默认调用此块
	lua_getglobal(mState, spacename.data());								  //想要使用的_ENV备用空间
	lua_setupvalue(mState, -2, 1);											  //将栈顶变量赋值给栈顶第二个函数的第一个upvalue(当前第二个函数为load返回的函数，第一个upvalue为_ENV),注：upvalue:函数的外部引用变量\
	                              //在赋值成功以后，栈顶自动回弹一层
	/****************************************************/
	//lua_getglobal(mState, "_G");
	//lua_getfield(mState, -1, spacename.data());
	//lua_setupvalue(mState, -3, 1);
	//lua_pop(mState, 1);//在赋值成功以后，栈顶自动回弹一层,当前栈顶指向“_G”,需要手动pop一层，指向lua_pcall需要呼叫的chunk块
	/**************************************************/
	sta |= lua_pcall(mState, 0, 0, 0);
	return sta ? false : true;
}

bool lua_scheduler::get_function(string spacename, string func)
{
	lua_getglobal(mState, spacename.data());
	if (lua_istable(mState, -1))
	{
		lua_getfield(mState, -1, func.data());
		if (lua_isfunction(mState, -1))
		{
			return true;
		}
	}
	return false;
}

 boost::optional<FunctionSummary> lua_scheduler::Reader<FunctionSummary>::read(lua_State *state, int index)
    {

        Closure *pt = (Closure *)lua_topointer(state, index);
        if (pt != 0 && ttisclosure((TValue *)pt) && pt->l.p != 0)
        {
            struct FunctionSummary sfunc;
            Proto *pro = pt->l.p;
            auto arg_num = (int)pro->numparams;
            sfunc.is_var_arg = (bool)pro->is_vararg;
            LocVar *var = pro->locvars;
            if (arg_num != 0 && arg_num <= pro->sizelocvars && var != nullptr)
            {
                LocVar *per = nullptr;
                for (int pos = 0; pos < arg_num; pos++)
                {
                    per = var + pos;
                    sfunc.arglist.push_back(getstr(per->varname));
                }
            }
            return sfunc;
        }
        return boost::none;
    }

} // namespace chain
} // namespace graphene