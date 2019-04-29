#include <LuaContext.hpp>
#include <gtest/gtest.h>

TEST(Metatables, WritingMetatable)
{
    LuaContext context;

    bool called = false;

    context.writeVariable("foo", LuaContext::EmptyArray);
    context.writeFunction("foo", LuaContext::Metatable, "__call", [&](const std::vector<std::pair<int, int>>&) { called = true; });
    EXPECT_EQ("table", context.executeCode<std::string>("return type(getmetatable(foo))"));

    context.executeCode("foo()");
    EXPECT_TRUE(called);
}

TEST(Metatables, ReadingMetatable)
{
    LuaContext context;

    context.writeVariable("foo", LuaContext::EmptyArray);
    context.writeVariable("foo", LuaContext::Metatable, "x", 18);
    
    EXPECT_EQ(18, context.executeCode<int>("return getmetatable(foo).x"));
    EXPECT_EQ(18, context.readVariable<int>("foo", LuaMetatable, "x"));
}

TEST(Metatables, WritingMetatableObjects)
{
    struct Foo { int value = 0; };

    LuaContext context;

    context.writeVariable("foo", Foo{});
    context.writeFunction("foo", LuaContext::Metatable, "__call", [](Foo& foo) { foo.value++; });
    context.writeFunction("foo", LuaContext::Metatable, "__index", [](Foo& foo, std::string index) -> int { foo.value += index.length(); return 12; });

    context.executeCode("foo()");
    EXPECT_EQ(12, context.executeCode<int>("return foo.test"));

    EXPECT_EQ(5, context.readVariable<Foo>("foo").value);
}
