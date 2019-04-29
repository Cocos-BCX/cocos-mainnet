#include <wrapper/LuaContext.hpp>
#include <gtest/gtest.h>

TEST(AdvancedReadWrite, WritingVariant) {
    LuaContext context;

    boost::variant<int,std::string> val{std::string{"test"}};
    context.writeVariable("ab", val);
    printf("WritingVariant:%s\r\n",context.readVariable<std::string>("ab").data());
    EXPECT_EQ("test", context.readVariable<std::string>("ab"));
}

TEST(AdvancedReadWrite, WritingMAP) {
    LuaContext context;

    context.writeVariable("a",
        std::vector< std::pair< std::string, std::string >>
        {
            { "test", "dasd" },
            { "hello","dasd" },
            { "world", "asd" },
        }
    );

    EXPECT_EQ("table", context.executeCode<std::string>("return type(a)"));
    EXPECT_EQ("dasd", context.executeCode<std::string>("b=a.test return b"));
    EXPECT_EQ("dasd", context.readVariable<std::string>("a", "world"));
}


TEST(AdvancedReadWrite, AdvancedExample) {
    LuaContext context;

    context.writeVariable("a",
        std::vector< std::pair< boost::variant<int,std::string>, boost::variant<bool,float> >>
        {
            { "test", true },
            { 2, 6.4f },
            { "hello", 1.f },
            { "world", -7.6f },
            { 18, false }
        }
    );

    EXPECT_EQ(true, context.executeCode<bool>("return a.test"));
    EXPECT_DOUBLE_EQ(6.4f, context.executeCode<float>("return a[2]"));
    EXPECT_DOUBLE_EQ(-7.6f, context.readVariable<float>("a", "world"));
}

TEST(AdvancedReadWrite, ReadingVariant) {
    LuaContext context;
    
    context.writeVariable("a", "test");

    const auto val = context.readVariable<boost::variant<bool,int,std::string>>("a");
    EXPECT_EQ(2, val.which());
    EXPECT_EQ("test", boost::get<std::string>(val));
}

TEST(AdvancedReadWrite, VariantError) {
    LuaContext context;
    
    context.writeVariable("a", "test");
    EXPECT_THROW((context.readVariable<boost::variant<bool,int,std::vector<double>>>("a")), LuaContext::WrongTypeException);
}

TEST(AdvancedReadWrite, ReadingOptional) {
    LuaContext context;
    
    context.writeVariable("a", 3);
    context.writeVariable("b", "test");

    EXPECT_EQ(3, context.readVariable<boost::optional<int>>("a").get());
    EXPECT_THROW(context.readVariable<boost::optional<int>>("b"), LuaContext::WrongTypeException);
    EXPECT_FALSE(context.readVariable<boost::optional<int>>("c").is_initialized());
}

TEST(AdvancedReadWrite, EmptyArray) {
    LuaContext context;

    context.writeVariable("a", LuaContext::EmptyArray);
    EXPECT_EQ("table", context.executeCode<std::string>("return type(a)"));
}

TEST(AdvancedReadWrite, ReadInsideArrays) {
    LuaContext context;

    context.executeCode("a = { 12, 34 }");
    EXPECT_EQ(12, context.readVariable<int>("a", 1));
    EXPECT_EQ(34, context.readVariable<int>("a", 2));
}

TEST(AdvancedReadWrite, WriteVariableInsideArrays) {
    LuaContext context;

    context.executeCode("a = { 1, {} }");
    context.writeVariable("a", 1, 34);
    context.writeVariable("a", 2, "test", 14);
    EXPECT_EQ(34, context.readVariable<int>("a", 1));
    EXPECT_EQ(14, context.readVariable<int>("a", 2, "test"));
}

TEST(AdvancedReadWrite, WriteFunctionInsideArrays) {
    LuaContext context;

    context.executeCode("a = { 1, {} }");
    context.writeFunction<int (int)>("a", 1, [](int x) { return x + 1; });
    context.writeFunction("a", 2, "test", [](int x) { return x * 2; });
    EXPECT_EQ(34, context.executeCode<int>("local f = a[1]; return f(33)"));
    EXPECT_EQ(14, context.executeCode<int>("local f = a[2].test; return f(7)"));
}

TEST(AdvancedReadWrite, WritingVectors) {
    LuaContext context;
    
    context.writeVariable("a", std::vector<std::string>{"hello", "world"});

    EXPECT_EQ("hello", context.readVariable<std::string>("a", 1));
    EXPECT_EQ("world", context.readVariable<std::string>("a", 2));

    const auto val = context.readVariable<std::map<int,std::string>>("a");
    EXPECT_EQ("hello", val.at(1));
    EXPECT_EQ("world", val.at(2));
}

TEST(AdvancedReadWrite, VectorOfPairs) {
    LuaContext context;
    
    context.writeVariable("a", std::vector<std::pair<int,std::string>>{
        { 1, "hello" },
        { -23, "world" }
    });

    EXPECT_EQ("hello", context.readVariable<std::string>("a", 1));
    EXPECT_EQ("world", context.readVariable<std::string>("a", -23));

    const auto val = context.readVariable<std::vector<std::pair<int,std::string>>>("a");
    EXPECT_TRUE(val[0].first == 1 || val[0].first == -23);
    EXPECT_TRUE(val[1].first == 1 || val[1].first == -23);
    EXPECT_TRUE(val[0].second == "hello" || val[0].second == "world");
    EXPECT_TRUE(val[1].second == "hello" || val[1].second == "world");
}

TEST(AdvancedReadWrite, Maps) {
    LuaContext context;
    
    context.writeVariable("a", std::map<int,std::string>{
        { 1, "hello" },
        { -23, "world" }
    });
    
    context.executeCode("b = { \"hello\", \"world\" }");

    EXPECT_EQ("hello", context.readVariable<std::string>("a", 1));
    EXPECT_EQ("world", context.readVariable<std::string>("a", -23));
    
    EXPECT_EQ("hello", context.readVariable<std::string>("b", 1));
    EXPECT_EQ("world", context.readVariable<std::string>("b", 2));

    const auto val = context.readVariable<std::map<int,std::string>>("a");
    EXPECT_EQ("hello", val.at(1));
    EXPECT_EQ("world", val.at(-23));
    
    const auto b = context.readVariable<std::map<int,std::string>>("b");
    EXPECT_EQ("hello", b.at(1));
    EXPECT_EQ("world", b.at(2));
}

TEST(AdvancedReadWrite, UnorderedMaps) {
    LuaContext context;
    
    context.writeVariable("a", std::unordered_map<int,std::string>{
        { 1, "hello" },
        { -23, "world" }
    });

    EXPECT_EQ("hello", context.readVariable<std::string>("a", 1));
    EXPECT_EQ("world", context.readVariable<std::string>("a", -23));

    const auto val = context.readVariable<std::unordered_map<int,std::string>>("a");
    EXPECT_EQ("hello", val.at(1));
    EXPECT_EQ("world", val.at(-23));
}

TEST(AdvancedReadWrite, WritingOptionals) {
    LuaContext context;

    context.writeVariable("a", boost::optional<int>{});
    context.writeVariable("b", boost::optional<int>{12});

    EXPECT_EQ("nil", context.executeCode<std::string>("return type(a)"));
    EXPECT_EQ("number", context.executeCode<std::string>("return type(b)"));
    EXPECT_EQ(12, context.executeCode<int>("return b"));
}
