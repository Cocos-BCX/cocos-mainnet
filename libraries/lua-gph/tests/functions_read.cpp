#include <LuaContext.hpp>
#include <gtest/gtest.h>

TEST(FunctionsRead, NativeFunctions) {
    struct Foo {
        static int increment(int x)
        {
            return x + 1;
        }
    };

    LuaContext context;
    
    context.writeVariable("f", &Foo::increment);

    const auto f = context.readVariable<LuaContext::LuaFunctionCaller<int (int)>>("f");
    EXPECT_EQ(4, f(3));
    
    const auto g = context.readVariable<std::function<int (int)>>("f");
    EXPECT_EQ(4, g(3));
}

TEST(FunctionsRead, Lambdas) {
    LuaContext context;
    
    context.writeFunction("f", [](int x) { return x + 1; });

    const auto f = context.readVariable<LuaContext::LuaFunctionCaller<int (int)>>("f");
    EXPECT_EQ(4, f(3));
    
    const auto g = context.readVariable<std::function<int (int)>>("f");
    EXPECT_EQ(4, g(3));
}

TEST(FunctionsRead, LuaFunctions) {
    LuaContext context;
    
    context.executeCode("f = function(x) return x + 1; end");

    const auto f = context.readVariable<LuaContext::LuaFunctionCaller<int (int)>>("f");
    EXPECT_EQ(4, f(3));

    const auto g = context.readVariable<std::function<int (int)>>("f");
    EXPECT_EQ(4, g(3));
}

TEST(FunctionsRead, LuaFunctionsCleanup) {
    LuaContext context;
    
    context.executeCode("f = function(x) return x + 1; end");
    const auto f = context.readVariable<std::function<int (int)>>("f");
    context.writeVariable("f", nullptr);
    EXPECT_EQ(4, f(3));
}

TEST(FunctionsRead, CallLuaFunctionFromWithinCallback) {
    LuaContext context;
    
    context.writeFunction("execute", [&](const std::string& varName) {
        const auto f = context.readVariable<std::function<int()>>(varName);
        EXPECT_EQ(5, f());
    });

    context.executeCode(R"(
        function test()
            return 5
        end
        
        execute("test")
    )");
}
