#include <gtest/gtest.h>
#include <string>
#include "stdx/singleton.hpp"

// A derived class which inherits from the Singleton.
// It can have private constructors that take arguments.
class MyService : public stdx::Singleton<MyService>
{
    friend class stdx::Singleton<MyService>;

private:
    // Default constructor
    MyService()
        : data_("default") {}

    // Constructor with a string argument
    explicit MyService(const std::string &val)
        : data_(val) {}

public:
    const std::string &data() const
    {
        return data_;
    }

protected:
    std::string data_;
};

//---------------------------------------------------
// TEST CASES
//---------------------------------------------------

TEST(SingletonTest, SameInstance_NoArgs)
{
    // First call: creates the singleton with default constructor
    MyService &service1 = MyService::create();
    // Second call: returns the existing instance
    MyService &service2 = MyService::create();

    // Check they're the same underlying object
    EXPECT_EQ(&service1, &service2);
    // Also verify the default constructor was used
    EXPECT_EQ("default", service1.data());

    // Cleanup
    MyService::destroy();
}

TEST(SingletonTest, ConstructorArgs_FirstCallWins)
{
    // First call to create(...) uses "Hello"
    MyService &service1 = MyService::create(std::string("Hello"));
    EXPECT_EQ("Hello", service1.data());

    // Second call with a different argument does NOT reconstruct
    MyService &service2 = MyService::create(std::string("Ignored"));
    // Same underlying object
    EXPECT_EQ(&service1, &service2);
    // Still "Hello"
    EXPECT_EQ("Hello", service2.data());

    MyService::destroy();
}

TEST(SingletonTest, DestroyAndRecreate)
{
    // Create with "Initial"
    {
        MyService &s1 = MyService::create(std::string("Initial"));
        EXPECT_EQ("Initial", s1.data());

        // Another create call with different arg is ignored
        MyService &s2 = MyService::create(std::string("Foo"));
        EXPECT_EQ(&s1, &s2);
        EXPECT_EQ("Initial", s2.data());
    }

    // Now explicitly destroy
    MyService::destroy();

    // The old instance is gone. Calling instance() should throw
    EXPECT_THROW({ MyService::instance(); }, std::runtime_error);

    // We can now create a fresh MyService with a new argument
    MyService &s3 = MyService::create(std::string("NewOne"));
    EXPECT_EQ("NewOne", s3.data());

    MyService::destroy();
}

TEST(SingletonTest, InstanceMethod_ThrowsIfNotCreated)
{
    // If we haven't called create(...) yet,
    // instance() should throw std::runtime_error
    EXPECT_THROW({ MyService::instance(); }, std::runtime_error);

    // Now create it
    MyService &service = MyService::create();
    EXPECT_NO_THROW({
        MyService &s2 = MyService::instance();
        // Same object
        EXPECT_EQ(&service, &s2);
    });

    MyService::destroy();
}

//---------------------------------------------------
// Main test runner
//---------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
