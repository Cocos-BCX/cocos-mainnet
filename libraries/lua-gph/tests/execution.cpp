#include <LuaContext.hpp>
#include <gtest/gtest.h>

TEST(Execution, BasicExecution) {
    LuaContext context;
    context.executeCode("a = 2");
    EXPECT_EQ(2, context.readVariable<int>("a"));
}

TEST(Execution, Errors) {
    LuaContext context;
    EXPECT_THROW(context.executeCode("qsdfqsdf"), LuaContext::SyntaxErrorException);

    context.writeFunction("f", [](bool) {});
    EXPECT_THROW(context.executeCode("f('hello')"), LuaContext::ExecutionErrorException);
}

TEST(Execution, ReturningValues) {
    LuaContext context;
    EXPECT_EQ(2, context.executeCode<int>("return 2"));
    EXPECT_EQ("hello", context.executeCode<std::string>("return 'hello'"));
    EXPECT_EQ(true, context.executeCode<bool>("return true"));
    
    const auto f = context.executeCode<std::function<int (int)>>("return function(x) return x + 1; end");
    EXPECT_EQ(5, f(4));
}

TEST(Execution, ReturningMultipleValues) {
    LuaContext context;

    const auto values = context.executeCode<std::tuple<int,std::string,int>>("return 2, 'hello', 5");
    EXPECT_EQ(2, std::get<0>(values));
    EXPECT_EQ("hello", std::get<1>(values));
    EXPECT_EQ(5, std::get<2>(values));
}
