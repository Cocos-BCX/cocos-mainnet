
#define lua_c

#include "lprefix.hpp"


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.hpp"

#include "lauxlib.hpp"
#include "lualib.hpp"
#include <wrapper/LuaContext.hpp>

#define Lua_code  " contract_global_variable={'ret',abc=1} \
                    a=0 b=1 \
                    contract_global_variable['1']=10\
                     contract_global_variable[1]=10\
                    contract_global_variable['sum']=0 \
                    contract_global_variable['nidaye']='dayenihao' \
                    function sum()          \
                       contract_global_variable['sum']= a+b+contract_global_variable['sum'] \
                       a=a+1 b=b+2 \
                       end"
#define Lua_main "function whiletrue() i=1  while(i) do  i=i+1 end end "
                    //构建表中元素(C -> stack)  
void setField(lua_State *L, const char *key, char * value)  
{  
    lua_pushstring(L, value);  
    lua_setfield(L, -2, key);  
} 


void get_data(lua_State *L,int index)
{
    int type = lua_type(L, index); 
    printf("type:%d\r\n",type);
    switch(type)  
    {  
        case LUA_TNIL:
        {
            printf("\r\nLUA_TNIL\r\n");  
                break;   
        }
        case LUA_TSTRING:  
            {  
                printf("\r\nLUA_TSTRING:%s\r\n",lua_tostring(L, index));  
                break;  
            }  
  
        case LUA_TBOOLEAN:  
            {  
                printf("\r\nLUA_TBOOLEAN:",lua_toboolean(L, index)?"true\r\n":"false\r\n");  
                break;  
            }  
        case LUA_TNUMBER:  
            {  
                printf("\r\nLUA_TNUMBER:%g\r\n",lua_tonumber(L, index));  
                break;  
            }  
        case LUA_TTABLE:  
            {  
                printf("this is a table!\r\n");  
                lua_pushnil( L );  // nil 入栈作为初始 key 
                while( 0 != lua_next( L, index ) ) 
                { 
                    // 现在栈顶(-1)是 value，-2 位置是对应的 key 
                    // 这里可以判断 key 是什么并且对 value 进行各种处理
                    get_data(L, index+1);
                    get_data(L, index+2); 
                    lua_pop( L, 1 );  // 弹出 value，让 key 留在栈顶 
                    
                }       
                break;  
            }  
        default:  
            {  
                printf("%s",lua_typename(L ,index));  
                break;  
            }  

    }
}
void stackDump(lua_State *L,int top)  
{  
    int i;  
    //printf("the size of stack is:%d\n",top);  
    //for ( i = 1;i <= top;i++ )  
    {  
        get_data(L,top);  
    }  
    printf("\n");  
} 

template<typename T>
char returnchar(T v)
{   
    char ret;
    memcap(&ret,&v,1);
    return ret;
}

template<typename ...T>
void print(T... agrs)
{
    char *p={returnchar(agrs)...};
    printf("test:%s\r\n",p);
}


class ca
{
public:
    int call_times;
    ca(int call_times):call_times(call_times){}
    
    void operator()(int s){
        printf("s print : %d\n", s);
        printf("call_times print : %d\n", call_times);
    }
};



template<typename Vector, std::size_t... I>
auto writer_lua_value(LuaContext& context,const Vector& a, std::index_sequence<I...>)
{
    return context.writeVariable(a[I]...);
}
template<typename T, std::size_t N >
struct writer_helper{
   static auto v2t(LuaContext& context, std::vector<T>& a)
    {
        if(a.size()==N) 
            return writer_lua_value(context,a,std::make_index_sequence<N>());
        else 
        return writer_helper<T,N-1>::v2t(context,a);
    }
};
template<typename T>
struct writer_helper<T,2>
{
   static auto v2t(LuaContext& context,std::vector<T>& a)
    {
        throw -1;
    }
};





#include <boost/bind.hpp>

int main()
{
    //Lua示例代码，使用table
  // auto a=ca(1);
  // a(2);
    char *szLua_code =Lua_code;
    char* main_code=Lua_main;
    
    //Lua的字符串模式
    char *szMode = "(%w+)%s*=%s*(%w+)";
    //要处理的字符串
    char *szStr = "key1 = value1 key2 = value2";
    //目标字符串模式
    char *szTag = "<%1>%2</%1>";
 /*...............................................*/
    LuaContext context;
    //lua_State *L = luaL_newstate();
    //luaL_openlibs(L);
    //luaopen_base(L);
    //luaL_openlibs(L);
    //执行
    //int err = luaL_dofile(L,"./lua.test");
    
    lua_State *L=context.mState;
    int err = luaL_loadbuffer(context.mState, szLua_code, strlen(szLua_code),"demo");
     err |= lua_pcall(L, 0, 0, 0);
    if(err)
    {
        //如果错误，显示
        printf("err::%s\r\n", lua_tostring(L, -1));
        //弹出栈顶的这个错误信息
        lua_pop(L, 1);
    }
    std::vector< boost::variant<int,std::string> > values;
    values.push_back(boost::variant<int,std::string>("contract_global_variable"));
    values.push_back(boost::variant<int,std::string>("sum"));
    values.push_back(boost::variant<int,std::string>(30));
   // auto mytuple =write_lua_table(values);
   writer_helper<boost::variant<int,std::string>,100>::v2t(context,values);
    //writer_lua_value(context,values,std::make_index_sequence<3>());
    //apply(boost::bind(&LuaContext::writeVariable,context),mytuple);

     //context.writeVariable("contract_global_variable",values[1],values[2]);
        lua_getglobal(L,"sum");
        //lua_pushinteger(L,1);
        //lua_pushinteger(L,2);
        err |=lua_pcall(L, 0, 0,0);
    if(err)
    {
        //如果错误，显示
        printf("err::%s\r\n", lua_tostring(L, -1));
        //弹出栈顶的这个错误信息
        lua_pop(L, 1);
    }

    
    context.writeVariable("t",LuaContext::EmptyArray);
    
    //for(size_t i = 0; i < count; i++)
    {
        context.writeVariable("t",1,"woshi nibaba");
        context.writeVariable("t","Mode",szMode);
        context.writeVariable("t","Tag",szTag);
        context.writeVariable("t","Str",szStr);
    }
    
   /*
    lua_newtable(L);    //新建一个table并压入栈顶
    lua_pushstring(L, "Mode");// key
    lua_pushstring(L, szMode);// value
    //设置newtable[Mode]=szMode
    //由于上面两次压栈，现在table元素排在栈顶往下数第三的位置
    lua_settable(L, -3);
    //lua_settable会自己弹出上面压入的key和value
 
    lua_pushstring(L, "Tag");// key
    lua_pushstring(L, szTag);// value
    lua_settable(L, -3);    //设置newtable[Tag]=szTag
 
    lua_pushstring(L, "Str");// key
    lua_pushstring(L, szStr);// value
    lua_settable(L, -3);    //设置newtable[Str]=szStr
 
    lua_setglobal(L,"c"); //将栈顶元素(newtable)置为Lua中的全局变量c
 *................................................/
    lua_getglobal(L,"sum");
    lua_pushinteger(L,1);
    lua_pushinteger(L,2);
    lua_pcall(L, 2, 1,0);
    */
   
    if(err==0)
    {
      printf("type:%s\r\n" ,context.executeCode<std::string>("return type(t)").data());
        
        // 现在栈顶是 table

        //Lua执行后取得全局变量的值
        lua_getglobal(L, "contract_global_variable");// Contract global variable 
        // 进行下面步骤前先将 table 压入栈顶 

        stackDump(L,lua_gettop(L));
        //这个x应该是个table
       
        //弹出栈顶的x
        lua_pop(L, 1);
    }
    //lua_close(L);
    return 0;
} 
