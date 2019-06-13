#include <LuaContext.hpp>
#include <gtest/gtest.h>

TEST(CustomTypes, ReadWrite) {
    struct Object {
        int value;
    };
    
    LuaContext context;
    context.writeVariable("obj", Object{5});
    EXPECT_EQ(5, context.readVariable<Object>("obj").value);
}

TEST(CustomTypes, ReadReference) {
    struct Object {
        int value;
    };
    
    LuaContext context;
    context.writeVariable("obj", Object{5});
    context.readVariable<Object&>("obj").value = 12;    
    EXPECT_EQ(12, context.readVariable<Object>("obj").value);
}

TEST(CustomTypes, MemberFunctions) {
    struct Object {
        void  increment() { ++value; } 
        int value;
    };
    
    LuaContext context;
    context.registerFunction("increment", &Object::increment);
    
    context.writeVariable("obj", Object{10});
    context.executeCode("obj:increment()");

    EXPECT_EQ(11, context.readVariable<Object>("obj").value);
}

TEST(CustomTypes, ConstMemberFunctionsWithRawPointers) {
    struct Object {
        int foo() { return 1; }
        int fooC() const { return 2; }
    };
    
    LuaContext context;
    context.registerFunction("foo", &Object::foo);
    context.registerFunction("fooC", &Object::fooC);
    
    Object obj;
    context.writeVariable("obj", &obj);
    context.writeVariable("objC", const_cast<Object const*>(&obj));

    EXPECT_EQ(2, context.executeCode<int>("return obj:fooC()"));
    EXPECT_EQ(1, context.executeCode<int>("return obj:foo()"));
    EXPECT_EQ(2, context.executeCode<int>("return objC:fooC()"));
    EXPECT_ANY_THROW(context.executeCode<int>("return objC:foo()"));
}

TEST(CustomTypes, ConstMemberFunctionsWithSharedPointers) {
    struct Object {
        int foo() { return 1; }
        int fooC() const { return 2; }
    };
    
    LuaContext context;
    context.registerFunction("foo", &Object::foo);
    context.registerFunction("fooC", &Object::fooC);
    
    auto obj = std::make_shared<Object>();
    context.writeVariable("obj", obj);
    context.writeVariable("objC", std::shared_ptr<const Object>(obj));

    EXPECT_EQ(2, context.executeCode<int>("return obj:fooC()"));
    EXPECT_EQ(1, context.executeCode<int>("return obj:foo()"));
    EXPECT_EQ(2, context.executeCode<int>("return objC:fooC()"));
    EXPECT_ANY_THROW(context.executeCode<int>("return objC:foo()"));
}

TEST(CustomTypes, ConstVolatileMemberFunctions) {
    struct Object {
        int foo() { return 1; }
        int fooC() const { return 2; }
        int fooV() volatile { return 3; }
        int fooCV() const volatile { return 4; }
    };
    
    LuaContext context;
    context.registerFunction("foo", &Object::foo);
    context.registerFunction("fooC", &Object::fooC);
    context.registerFunction("fooV", &Object::fooV);
    context.registerFunction("fooCV", &Object::fooCV);
    
    context.writeVariable("obj", Object{});

    EXPECT_EQ(1, context.executeCode<int>("return obj:foo()"));
    EXPECT_EQ(2, context.executeCode<int>("return obj:fooC()"));
    EXPECT_EQ(3, context.executeCode<int>("return obj:fooV()"));
    EXPECT_EQ(4, context.executeCode<int>("return obj:fooCV()"));
}

TEST(CustomTypes, MemberFunctionsReturnedObjects) {
    struct Object {
        int add(int x) { return x + 2; } 
    };
    
    LuaContext context;
    context.registerFunction("add", &Object::add);
    
    context.writeVariable("Object", LuaContext::EmptyArray);
    context.writeFunction("Object", "newRaw", []() { return Object{}; });
    Object obj;
    context.writeFunction("Object", "newPtr", [&]() { return &obj; });
    context.writeFunction("Object", "newSharedPtr", []() { return std::make_shared<Object>(); });
    
    EXPECT_EQ(12, context.executeCode<int>("return Object.newRaw():add(10)"));
    EXPECT_EQ(17, context.executeCode<int>("return Object.newPtr():add(15)"));
    EXPECT_EQ(22, context.executeCode<int>("return Object.newSharedPtr():add(20)"));
}

TEST(CustomTypes, MembersPlain) {
    struct Object {
        int value;
    };
    
    LuaContext context;
    context.registerMember("value", &Object::value);
    
    context.writeVariable("obj", Object{10});
    context.executeCode("obj.value = obj.value + 5");

    EXPECT_EQ(15, context.readVariable<Object>("obj").value);
}

TEST(CustomTypes, MembersPointers) {
    struct Object {
        int value;
    };
    
    LuaContext context;
    context.registerMember("value", &Object::value);
    
    Object obj{10};
    context.writeVariable("obj", &obj);
    context.executeCode("obj.value = obj.value + 5");
    EXPECT_EQ(15, obj.value);
    
    context.writeVariable("obj2", const_cast<Object const*>(&obj));
    EXPECT_EQ(15, context.executeCode<int>("return obj2.value"));
    EXPECT_ANY_THROW(context.executeCode("obj2.value = 12"));
}

TEST(CustomTypes, MembersSharedPointers) {
    struct Object {
        int value;
    };
    
    LuaContext context;
    context.registerMember("value", &Object::value);
    
    auto obj = std::make_shared<Object>();
    obj->value = 10;
    context.writeVariable("obj", obj);
    context.executeCode("obj.value = obj.value + 5");
    EXPECT_EQ(15, obj->value);
    
    context.writeVariable("obj2", std::shared_ptr<Object const>(obj));
    EXPECT_EQ(15, context.executeCode<int>("return obj2.value"));
    EXPECT_ANY_THROW(context.executeCode("obj2.value = 12"));
}

TEST(CustomTypes, CustomMemberFunctions) {
    struct Object {
        Object(int v) : value(v) {}
        int value;
    };
    
    LuaContext context;
    context.registerFunction<void (Object::*)()>("increment", [](Object& obj) { ++obj.value; });
    context.registerFunction<Object, int (int)>("add", [](Object& obj, int x) { obj.value += x; return obj.value; });
    
    context.writeVariable("obj1", Object{10});
    Object obj{10};
    context.writeVariable("obj2", &obj);
    context.writeVariable("obj3", std::make_shared<Object>(10));

    context.executeCode("obj1:increment()");
    context.executeCode("obj2:increment()");
    context.executeCode("obj3:increment()");

    EXPECT_EQ(11, context.readVariable<Object>("obj1").value);
    EXPECT_EQ(11, context.readVariable<Object*>("obj2")->value);
    EXPECT_EQ(11, obj.value);
    EXPECT_EQ(11, context.readVariable<std::shared_ptr<Object>>("obj3")->value);

    EXPECT_EQ(14, context.executeCode<int>("return obj1:add(3)"));
    EXPECT_EQ(14, context.executeCode<int>("return obj2:add(3)"));
    EXPECT_EQ(14, context.executeCode<int>("return obj3:add(3)"));
}

TEST(CustomTypes, CustomMemberFunctionsCustomFunctionObject) {
    struct Object {
        Object(int v) : value(v) {}
        int value;
    };

    struct ObjectIncrementer {
        void operator()(Object& obj) const { ++obj.value; }
    };

    LuaContext context;
    context.registerFunction<void (Object::*)()>("increment1", ObjectIncrementer{});
    context.registerFunction<Object, void ()>("increment2", ObjectIncrementer{});

    context.writeVariable("obj1", Object{ 10 });
    Object obj{ 10 };
    context.writeVariable("obj2", &obj);
    context.writeVariable("obj3", std::make_shared<Object>(10));

    context.executeCode("obj1:increment1()");
    context.executeCode("obj1:increment2()");
    context.executeCode("obj2:increment1()");
    context.executeCode("obj2:increment2()");
    context.executeCode("obj3:increment1()");
    context.executeCode("obj3:increment2()");

    EXPECT_EQ(12, context.readVariable<Object>("obj1").value);
    EXPECT_EQ(12, context.readVariable<Object*>("obj2")->value);
    EXPECT_EQ(12, context.readVariable<std::shared_ptr<Object>>("obj3")->value);
}

TEST(CustomTypes, CustomMembers) {
    struct Object {};
    
    LuaContext context;
    context.registerMember<int (Object::*)>("value",
        [](const Object& obj) { return 2; },
        [](Object& obj, int val) {}
    );
    
    context.writeVariable("obj", Object{});
    EXPECT_EQ(2, context.executeCode<int>("return obj.value"));
}

TEST(CustomTypes, Unregistering) {
    struct Object {
        int  foo() { return 2; }
    };
    
    LuaContext context;
    context.writeVariable("obj", Object{});
    EXPECT_ANY_THROW(context.executeCode("return obj:foo()"));

    context.registerFunction("foo", &Object::foo);
    EXPECT_EQ(2, context.executeCode<int>("return obj:foo()"));

    context.unregisterFunction<Object>("foo");
    EXPECT_ANY_THROW(context.executeCode("return obj:foo()"));
}

TEST(CustomTypes, GenericMembers) {
    struct Object {
        int value = 5;
    };
    
    LuaContext context;
    context.registerMember<int (Object::*)>(
        [](const Object& obj, const std::string& name) { return obj.value; },
        [](Object& obj, const std::string& name, int val) { obj.value = val; }
    );
    
    context.writeVariable("obj", Object{});
    EXPECT_EQ(5, context.executeCode<int>("return obj.foo"));
    context.executeCode("obj.bar = 18");
    EXPECT_EQ(18, context.executeCode<int>("return obj.bowl"));
}

TEST(CustomTypes, CopiesCheckReadWrite) {
    int copiesCount = 0;
    int movesCount = 0;

    struct Foo {
        Foo(int* copiesCount, int* movesCount) : copiesCount(copiesCount), movesCount(movesCount) {}
        Foo(const Foo& f) : copiesCount(f.copiesCount), movesCount(f.movesCount) { ++*copiesCount; }
        Foo(Foo&& f) : copiesCount(f.copiesCount), movesCount(f.movesCount) { ++*movesCount; }

        int* copiesCount;
        int* movesCount;
    };


    LuaContext context;
    context.writeVariable("obj", Foo{&copiesCount, &movesCount});
    EXPECT_EQ(0, copiesCount);
    EXPECT_EQ(1, movesCount);

    context.readVariable<Foo>("obj");
    EXPECT_EQ(1, copiesCount);
    EXPECT_EQ(1, movesCount);
    
    context.executeCode("a = obj");
    EXPECT_EQ(1, copiesCount);
    EXPECT_EQ(1, movesCount);

    context.readVariable<Foo&>("obj");
    EXPECT_EQ(1, copiesCount);
    EXPECT_EQ(1, movesCount);
}

TEST(CustomTypes, CopiesCheckReturnByValue) {
    int copiesCount = 0;
    int movesCount = 0;
    
    struct Foo {
        Foo(int* copiesCount, int* movesCount) : copiesCount(copiesCount), movesCount(movesCount) {}
        Foo(const Foo& f) : copiesCount(f.copiesCount), movesCount(f.movesCount) { ++*copiesCount; }
        Foo(Foo&& f) : copiesCount(f.copiesCount), movesCount(f.movesCount) { ++*movesCount; }

        int* copiesCount;
        int* movesCount;
    };
    
    
    LuaContext context;
    context.writeFunction("build", [&]() { return Foo{&copiesCount, &movesCount}; });
    
    context.executeCode("obj = build()");
    EXPECT_EQ(0, copiesCount);
    EXPECT_EQ(movesCount, 1);
}
