#include <LuaContext.hpp>
#include <functional>
#include <gtest/gtest.h>
using namespace std::placeholders;
TEST(FunctionsWrite, NativeFunctions) {
    struct Foo {
        static int increment(int x)
        {
            return x + 1;
        }
        int print_sum(int x,int y)
        {
            return x + y;
        }

    };
    Foo foo;
    LuaContext context;
    auto f3 = std::bind(&Foo::print_sum, &foo, 95, _1);
    printf("\r\n%d\r\n",f3(5));
    context.writeVariable("f", &Foo::increment);
    context.writeFunction<int (int)>("g", &Foo::increment);
    context.writeFunction("h", &Foo::increment);
    context.writeFunction("f3", (std::function<int(int)>)std::bind(&Foo::print_sum, &foo, 95, _1));

    EXPECT_EQ(3, context.executeCode<int>("return f(2)"));
    EXPECT_EQ(13, context.executeCode<int>("return g(12)"));
    EXPECT_EQ(100, context.executeCode<int>("return f3(5)"));
    EXPECT_EQ(9, context.executeCode<int>("return h(8)"));
    EXPECT_THROW(context.executeCode<int>("return f(true)"), LuaContext::ExecutionErrorException);
}

TEST(FunctionsWrite, ConstRefParameters)
{
    struct Foo {
        static int length(const std::string& x)
        {
            EXPECT_EQ("test", x);
            return x.length();
        }
    };

    LuaContext context;
    
    context.writeVariable("f", &Foo::length);
    EXPECT_EQ(4, context.executeCode<int>("return f('test')"));
}

TEST(FunctionsWrite, VariantParameters)
{
    LuaContext context;

    struct Foo {};
    context.writeVariable("foo", Foo{});
    context.writeFunction("f", [](const boost::variant<int, Foo&>& val) { return val.which(); });

    EXPECT_EQ(1, context.executeCode<int>("return f(foo)"));
    EXPECT_EQ(0, context.executeCode<int>("return f(3)"));
}

TEST(FunctionsWrite, FunctionObjects) {
    struct Foo {
        int operator()(int x) {
            return x + 1;
        }

        double operator()(double) {
            EXPECT_TRUE(false);
            return 0;
        }
    };


    LuaContext context;
    
    context.writeVariable("f", std::function<int (int)>(Foo{}));
    context.writeFunction<int (int)>("g", Foo{});

    EXPECT_EQ(3, context.executeCode<int>("return f(2)"));
    EXPECT_EQ(13, context.executeCode<int>("return g(12)"));
}

TEST(FunctionsWrite, FunctionObjectsConst) {
    struct Foo {
        int operator()(int x) {
            return x + 1;
        }
        
        int operator()(int x) const {
            return x + 1;
        }
    };


    LuaContext context;
    
    context.writeVariable("f", std::function<int (int)>(Foo{}));
    context.writeFunction<int (int)>("g", Foo{});

    EXPECT_EQ(3, context.executeCode<int>("return f(2)"));
    EXPECT_EQ(13, context.executeCode<int>("return g(12)"));
}

TEST(FunctionsWrite, FunctionObjectsAutodetect) {
    struct Foo {
        int operator()(int x) {
            return x + 1;
        }
    };


    LuaContext context;
    
    context.writeVariable("f", std::function<int (int)>(Foo{}));
    context.writeFunction<int (int)>("g", Foo{});
    context.writeFunction("h", Foo{});

    EXPECT_EQ(3, context.executeCode<int>("return f(2)"));
    EXPECT_EQ(13, context.executeCode<int>("return g(12)"));
    EXPECT_EQ(9, context.executeCode<int>("return h(8)"));
}

TEST(FunctionsWrite, Lambdas) {
    LuaContext context;
    
    const auto konst = 1;
    const auto lambda = [&](int x) { return x + konst; };
    context.writeVariable("f", std::function<int (int)>(lambda));
    context.writeFunction<int (int)>("g", lambda);
    context.writeFunction("h", lambda);

    EXPECT_EQ(3, context.executeCode<int>("return f(2)"));
    EXPECT_EQ(13, context.executeCode<int>("return g(12)"));
    EXPECT_EQ(9, context.executeCode<int>("return h(8)"));
}

TEST(FunctionsWrite, DestructorCalled) {
    struct Foo {
        int operator()(int x) {
            return x + 1;
        }

        std::shared_ptr<char> dummy;
    };

    auto foo = Foo{ std::make_shared<char>() };
    std::weak_ptr<char> dummy = foo.dummy;

    auto context = std::make_shared<LuaContext>();
    context->writeFunction("f", foo);
    foo.dummy.reset();

    EXPECT_FALSE(dummy.expired());
    context.reset();
    EXPECT_TRUE(dummy.expired());
}

TEST(FunctionsWrite, ReturningMultipleValues) {
    LuaContext context;
    
    context.writeFunction("f", [](int x) { return std::make_tuple(x, x+1, "hello"); });
    context.executeCode("a, b, c = f(2)");

    EXPECT_EQ(2, context.readVariable<int>("a"));
    EXPECT_EQ(3, context.readVariable<int>("b"));
    EXPECT_EQ("hello", context.readVariable<std::string>("c"));
}

TEST(FunctionsWrite, PolymorphicFunctions) {
    LuaContext context;
    
    context.writeFunction("f",
        [](boost::variant<int,bool,std::string> x) -> std::string
        {
            if (x.which() == 0)
                return "int";
            else if (x.which() == 1)
                return "bool";
            else
                return "string";
        }
    );

    EXPECT_EQ("int", context.executeCode<std::string>("return f(2)"));
    EXPECT_EQ("bool", context.executeCode<std::string>("return f(true)"));
    EXPECT_EQ("string", context.executeCode<std::string>("return f('test')"));
}

TEST(FunctionsWrite, VariadicFunctions) {
    LuaContext context;

    context.writeFunction("f",
        [](int a, boost::optional<int> b, boost::optional<double> c) -> int {
            return c.is_initialized() ? 3 : (b.is_initialized() ? 2 : 1);
        }
    );
    
    EXPECT_EQ(1, context.executeCode<int>("return f(12)"));
    EXPECT_EQ(2, context.executeCode<int>("return f(12, 24)"));
    EXPECT_THROW(context.executeCode<int>("return f(12, 24, \"hello\")"), LuaContext::ExecutionErrorException);
    EXPECT_EQ(3, context.executeCode<int>("return f(12, 24, 3.5)"));
    
    context.writeFunction("g", [](boost::optional<int> a, boost::optional<int> b, int c) -> int { return 3; });
    EXPECT_EQ(3, context.executeCode<int>("return g(10, 20, 30)"));
    EXPECT_THROW(context.executeCode<int>("return g(12, 24)"), LuaContext::ExecutionErrorException);
}

TEST(FunctionsWrite, AccessLuaFromWithinCallback) {
    LuaContext context;
    
    context.writeFunction("read", [&](const std::string& varName) {
        EXPECT_EQ(5, context.readVariable<int>(varName));
    });

    context.executeCode("x = 5; read(\"x\");");
}

TEST(FunctionsWrite, ExecuteLuaFromWithinCallback) {
    LuaContext context;
    
    context.writeFunction("exec", [&](const std::string& varName) {
        EXPECT_EQ("x", varName);
        context.executeCode("x = 10");
        EXPECT_EQ(10, context.readVariable<int>(varName));
    });

    context.executeCode("exec(\"x\")");
}
