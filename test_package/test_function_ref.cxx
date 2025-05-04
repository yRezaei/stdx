#include <gtest/gtest.h>
#include <string>
#include <functional>
#include "stdx/function_ref.hpp"

using namespace stdx;

// Test fixture
class FunctionRefTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test with free function
int add(int a, int b) {
    return a + b;
}

TEST_F(FunctionRefTest, FreeFunctionTest) {
    FunctionRef<int(int, int)> ref = add;
    EXPECT_EQ(ref(2, 3), 5);
}

// Test with lambdas
TEST_F(FunctionRefTest, LambdaTest) {
    auto lambda = [](int a, int b) { return a * b; };
    FunctionRef<int(int, int)> ref = lambda;
    EXPECT_EQ(ref(2, 3), 6);
}

// Test with stateful lambdas
TEST_F(FunctionRefTest, StatefulLambdaTest) {
    int multiplier = 10;
    auto lambda = [&multiplier](int a) { return a * multiplier; };
    
    FunctionRef<int(int)> ref = lambda;
    EXPECT_EQ(ref(5), 50);
    
    // Change the state
    multiplier = 20;
    EXPECT_EQ(ref(5), 100);
}

// Test with function objects
class Multiplier {
public:
    explicit Multiplier(int factor) : factor_(factor) {}
    
    int operator()(int value) const {
        return value * factor_;
    }
    
private:
    int factor_;
};

TEST_F(FunctionRefTest, FunctionObjectTest) {
    Multiplier mul(7);
    FunctionRef<int(int)> ref = mul;
    EXPECT_EQ(ref(6), 42);
}

// Test empty/reset
TEST_F(FunctionRefTest, EmptyTest) {
    FunctionRef<void()> ref;
    EXPECT_FALSE(bool(ref));
    
    auto lambda = []() {};
    ref = lambda;
    EXPECT_TRUE(bool(ref));
    
    ref.reset();
    EXPECT_FALSE(bool(ref));
    EXPECT_THROW(ref(), std::bad_function_call);
}

// Test lifetime management (this demonstrates why lifetime must be managed)
TEST_F(FunctionRefTest, LifetimeTest) {
    FunctionRef<int()> ref;
    
    {
        int value = 42;
        auto lambda = [&value]() { return value; };
        ref = lambda;
        EXPECT_EQ(ref(), 42);
    }
    
    // Now lambda is out of scope, ref is dangling
    // This would cause undefined behavior if called:
    // ref();  // BAD: lambda has been destroyed
}

// Test with std::function
TEST_F(FunctionRefTest, StdFunctionTest) {
    std::function<int(int, int)> func = [](int a, int b) { return a + b; };
    FunctionRef<int(int, int)> ref = func;
    EXPECT_EQ(ref(10, 20), 30);
}

// Test with mutable lambda
TEST_F(FunctionRefTest, MutableLambdaTest) {
    int counter = 0;
    auto lambda = [counter]() mutable { return ++counter; };
    
    FunctionRef<int()> ref = lambda;
    EXPECT_EQ(ref(), 1);
    EXPECT_EQ(ref(), 2);
    EXPECT_EQ(ref(), 3);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}