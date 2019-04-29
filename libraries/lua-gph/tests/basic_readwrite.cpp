#include <wrapper/LuaContext.hpp>
#include <gtest/gtest.h>

TEST(BasicReadWrite, Integers) {
    LuaContext context;

    context.writeVariable("a", 5);
    EXPECT_EQ(5, context.readVariable<int>("a"));
    
    context.writeVariable("b", -397);
    EXPECT_EQ(-397, context.readVariable<int>("b"));
}

TEST(BasicReadWrite, Doubles) {
    LuaContext context;

    context.writeVariable("a", 5.4);
    EXPECT_DOUBLE_EQ(5.4, context.readVariable<double>("a"));
    
    context.writeVariable("b", -6.72f);
    EXPECT_DOUBLE_EQ(-6.72f, context.readVariable<float>("b"));
}

TEST(BasicReadWrite, Booleans) {
    LuaContext context;

    context.writeVariable("a", true);
    EXPECT_TRUE(context.readVariable<bool>("a"));
    
    context.writeVariable("b", false);
    EXPECT_FALSE(context.readVariable<bool>("b"));
}

TEST(BasicReadWrite, Strings) {
    LuaContext context;

    context.writeVariable("a", "hello");
    EXPECT_EQ("hello", context.readVariable<std::string>("a"));
    
    context.writeVariable("b", "world");
    EXPECT_EQ("world", context.readVariable<std::string>("b"));
}

TEST(BasicReadWrite, Enums) {
    enum class Foo {
        A,
        B,
        C
    };

    LuaContext context;

    context.writeVariable("a", Foo::A);
    EXPECT_EQ(Foo::A, context.readVariable<Foo>("a"));
    
    context.writeVariable("b", Foo::B);
    EXPECT_EQ(Foo::B, context.readVariable<Foo>("b"));
}

TEST(BasicReadWrite, Conversions) {
    LuaContext context;

    context.writeVariable("a", "12");
    EXPECT_EQ(12, context.readVariable<int>("a"));
    
    context.writeVariable("b", 24);
    EXPECT_EQ("24", context.readVariable<std::string>("b"));
}

TEST(BasicReadWrite, TypeError) {
    struct Foo {};

    LuaContext context;

    context.writeVariable("a", "hello");
    EXPECT_THROW(context.readVariable<bool>("a"), LuaContext::WrongTypeException);
    EXPECT_THROW(context.readVariable<int>("a"), LuaContext::WrongTypeException);
    EXPECT_THROW(context.readVariable<double>("a"), LuaContext::WrongTypeException);
    EXPECT_THROW(context.readVariable<std::function<void ()>>("a"), LuaContext::WrongTypeException);
    EXPECT_THROW(context.readVariable<Foo>("a"), LuaContext::WrongTypeException);
    EXPECT_THROW(context.readVariable<std::vector<int>>("a"), LuaContext::WrongTypeException);
}
