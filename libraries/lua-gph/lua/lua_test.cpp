/*
#define lua_c

#include "lprefix.hpp"


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.hpp"

#include "lauxlib.hpp"
#include "lualib.hpp"

#define Lua_code  " contract_global_variable={}\
                    c={}            \
                    c.Str='key1 = 2a key2 = 2b'         \
                    c.Mode='(%w+)%s*=%s*(%w+)'                  \
                    c.Tag='<%1>%2</%1>'                         \
                    print('welcome') \
                    x = {} \
                    x.y={}\
                    x.y.a=100\
                    x[1],x[2] = string.gsub(c.Str, c.Mode, c.Tag) \
                    x.u = string.upper(x[1])    \
                    contract_global_variable.x=x\
                    contract_global_variable.c=c\
                    function sum (a,b)          \
                       contract_global_variable.sum= a+b \
                       end"
#define Lua_main "gsub()"
                    //构建表中元素(C -> stack)  
void setField(lua_State *L, const char *key, char * value)  
{  
    lua_pushstring(L, value);  
    lua_setfield(L, -2, key);  
} 


void get_data(lua_State *L,int index)
{
    int type = lua_type(L, index); 
    switch(type)  
    {  
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
    printf("the size of stack is:%d\n",top);  
    //for ( i = 1;i <= top;i++ )  
    {  
        get_data(L,top);  
    }  
    printf("\n");  
}  
int main()
{
    //Lua示例代码，使用table
    char *szLua_code =Lua_code;
    char* main_code=Lua_main;
   /* //Lua的字符串模式
    char *szMode = "(%w+)%s*=%s*(%w+)";
    //要处理的字符串
    char *szStr = "key1 = value1 key2 = value2";
    //目标字符串模式
    char *szTag = "<%1>%2</%1>";
 /*.............................................../
    lua_State *L = luaL_newstate();
    //luaL_openlibs(L);
    luaopen_base(L);
    luaL_openlibs(L);
    //执行
    //int err = luaL_dofile(L,"./lua.test");
    
    int err = luaL_loadbuffer(L, szLua_code, strlen(szLua_code),"demo");
    printf("asd:%d\r\n",err);
    lua_pcall(L, 0, 0, 0);
     //把一个tabele送给Lua
  
   // lua_newtable(L);  
   // setField(L, "Mode", szMode);  
   // setField(L, "Tag", szTag);  
   // setField(L, "Str", szStr);  
   // lua_setglobal(L, "c");  
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
    //err |= lua_pcall(L, 0, 0, 0);
    if(err)
    {
        //如果错误，显示
        printf("err::%s\r\n", lua_tostring(L, -1));
        //弹出栈顶的这个错误信息
        lua_pop(L, 1);
    }
    else
    {
       
        
        // 现在栈顶是 table

        //Lua执行后取得全局变量的值
        lua_getglobal(L, "contract_global_variable");// Contract global variable 
        // 进行下面步骤前先将 table 压入栈顶 

        stackDump(L,lua_gettop(L));
        //这个x应该是个table
       
        //弹出栈顶的x
        lua_pop(L, 1);
    }
    lua_close(L);
    return 0;
} 
*/
#include <stdio.h>
#include <string.h>
#include <string>
#include <lua_extern.hpp>
#include <lauxlib.hpp>
#include <lualib.hpp>
#include <wrapper/LuaContext.hpp>
#include <vector>
using namespace std;







//待Lua调用的C注册函数。
 int add2(lua_State* L)
{
    //检查栈中的参数是否合法，1表示Lua调用时的第一个参数(从左到右)，依此类推。
    //如果Lua代码在调用时传递的参数不为number，该函数将报错并终止程序的执行。
    double op1 = luaL_checknumber(L,1);
    double op2 = luaL_checknumber(L,2);
    //将函数的结果压入栈中。如果有多个返回值，可以在这里多次压入栈中。
    lua_pushnumber(L,op1 + op2);
    //返回值用于提示该C函数的返回值数量，即压入栈中的返回值数量。
    return 1;
}

typedef struct student{
    public:
        string name="dasd";
        vector<char> info;
}student;
//另一个待Lua调用的C注册函数。
 int sub2(lua_State* L)
{
    double op1 = luaL_checknumber(L,1);
    double op2 = luaL_checknumber(L,2);
    lua_pushnumber(L,op1 - op2);
    return 1;
}

typedef int(*function_ptr)(lua_State*);

int new_database_to_lua(lua_State* L,student &u,string name )
{
    student *p = (student*)lua_newuserdata(L, sizeof(student));
    memcpy(p,&u,sizeof(student));
    luaL_getmetatable(L, name.data());
    lua_setmetatable(L, -2);
    return 1;
}

const string  code = "f(val_lua)";

int main()
{
  LuaContext context;

    context.writeVariable("a", 5);
    context.writeVariable("tab_test",
        std::vector< std::pair< std::string, std::string >>
        {
            { "test", "dasd" },
            { "hello","dasd" },
            { "world", "asd" },
        }
    );
    student val=student();
    context.writeVariable("val_lua",val);
    context.writeFunction("f", [&val](student&val_lua) { 
                       val.name="wobianle";  
                       val_lua.name="woyebianle";             
     });
    
    context.writeVariable("b", -397);
    context.executeCode(code);
    printf("b:%d\r\n", context.readVariable<int>("b"));
    printf("val:%s\r\n", val.name.data());
    printf("val:%s\r\n", context.readVariable<student>("val_lua").name.data());
    #ifndef LUA_COMPAT_GRAPHENE
    context.executeCode("print(cjson.encode(tab_test))"); 
    #endif
    struct Foo { int value = 0; };

    context.writeVariable("foo", Foo{});
    context.writeFunction("foo", LuaContext::Metatable, "__call", [](Foo& foo) { foo.value++; });
    context.writeFunction("foo", LuaContext::Metatable, "__index", [](Foo& foo, std::string index) -> int { foo.value += index.length(); return 12; });
    context.writeFunction("foo", LuaContext::Metatable, "daxia", [](Foo& foo) -> int { foo.value--; return foo.value; });
      
}
