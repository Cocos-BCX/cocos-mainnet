#include <LuaContext.hpp>
#include <gtest/gtest.h>

TEST(Threads, Basics) {
    LuaContext context;
    context.writeVariable("a", "hello");

    auto thread1 = context.createThread();
    auto thread2 = context.createThread();

    context.executeCode(thread1, "a = 3");
    EXPECT_EQ(3, context.readVariable<int>("a"));
    EXPECT_EQ(3, context.readVariable<int>(thread2, "a"));
    context.destroyThread(thread1);

    context.executeCode(thread2, "a = 18");
    EXPECT_EQ(18, context.readVariable<int>("a"));
    EXPECT_EQ(18, context.readVariable<int>(thread1, "a"));

    context.writeVariable("a", "hello");
    EXPECT_EQ("hello", context.readVariable<std::string>(thread1, "a"));
    EXPECT_EQ("hello", context.readVariable<std::string>(thread2, "a"));
}
