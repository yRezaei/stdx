#include <gtest/gtest.h>
#include "stdx/light_callable.hpp"
#include <stdexcept>
#include <memory>

// Test fixture for LightCallable
class LightCallableTest : public ::testing::Test
{
protected:
    // Setup and teardown can be added here if needed
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: Small, copyable lambda with mutable state
TEST_F(LightCallableTest, SmallCopyableLambdaWithState)
{

    stdx::LightCallable<int()> callable = [count = 0]() mutable
    { return ++count; std::cout << count << std::endl; };

    EXPECT_EQ(callable(), 1); // count = 1
    EXPECT_EQ(callable(), 2); // count = 2

    auto callable_copy = callable; // Copies the lambda with count = 2

    EXPECT_EQ(callable(), 3);      // Original: count = 3
    EXPECT_EQ(callable_copy(), 3); // Copy: count = 3 (independent state)
    EXPECT_EQ(callable(), 4);      // Original: count = 4
    EXPECT_EQ(callable_copy(), 4); // Copy: count = 4
}

// Test 2: Large, mutable functor (heap-allocated, shared between copies)
struct LargeFunctorMutable
{
    int data[10]; // Larger than typical SBO buffer size (e.g., 32 bytes)
    int operator()(int x)
    {
        data[0] += x;
        return data[0];
    }
};

TEST_F(LightCallableTest, LargeFunctorMutable)
{
    LargeFunctorMutable lf;
    lf.data[0] = 100;
    stdx::LightCallable<int(int)> callable = lf;
    EXPECT_EQ(callable(5), 105); // data[0] = 105

    // Copy the callable (should share the same heap instance)
    auto callable_copy = callable;
    EXPECT_EQ(callable_copy(10), 115); // data[0] = 115
    EXPECT_EQ(callable(1), 116);       // data[0] = 116 (shared state)
}

// Test 3: Non-copyable functor (movable, heap-allocated)
struct NonCopyableFunctor
{
    std::unique_ptr<int> uptr;
    int count = 0;
    NonCopyableFunctor(int val) : uptr(std::make_unique<int>(val)) {}
    NonCopyableFunctor(const NonCopyableFunctor &) = delete; // Non-copyable
    NonCopyableFunctor(NonCopyableFunctor &&) = default;     // Movable

    int operator()()
    {
        return ++count;
    }
};

TEST_F(LightCallableTest, NonCopyableFunctor)
{
    stdx::LightCallable<int()> callable = NonCopyableFunctor(42); // Moved into callable
    EXPECT_EQ(callable(), 1);                               // count = 1
    EXPECT_EQ(callable(), 2);                               // count = 2

    // Copy the LightCallable (shares the heap instance)
    auto callable_copy = callable;
    EXPECT_EQ(callable_copy(), 3); // count = 3 (shared state)
    EXPECT_EQ(callable(), 4);      // count = 4
}

// Test 4: Moving a small callable
TEST_F(LightCallableTest, MoveSmallCallable)
{
    stdx::LightCallable<int()> callable = [count = 0]() mutable
    { return ++count; };
    EXPECT_EQ(callable(), 1); // count = 1

    // Move the callable
    auto moved_callable = std::move(callable);
    EXPECT_EQ(moved_callable(), 2); // count = 2

    // Original should be in an invalid state
    EXPECT_FALSE(static_cast<bool>(callable));
    EXPECT_THROW(callable(), std::bad_function_call);
}

// Test 5: Moving a large callable
TEST_F(LightCallableTest, MoveLargeCallable)
{
    struct LargeFunctor
    {
        int data[10];
        int operator()() { return data[0]; }
    };
    stdx::LightCallable<int()> callable = LargeFunctor{{42}};
    EXPECT_EQ(callable(), 42);

    // Move the callable
    auto moved_callable = std::move(callable);
    EXPECT_EQ(moved_callable(), 42);

    // Original should be in an invalid state
    EXPECT_FALSE(static_cast<bool>(callable));
    EXPECT_THROW(callable(), std::bad_function_call);
}

// Test 6: Empty callable
TEST_F(LightCallableTest, EmptyCallable)
{
    stdx::LightCallable<int()> callable; // Default-constructed
    EXPECT_FALSE(static_cast<bool>(callable));
    EXPECT_THROW(callable(), std::bad_function_call);
}

// Test 7: Exception propagation
TEST_F(LightCallableTest, ExceptionPropagation)
{
    stdx::LightCallable<void()> callable = []()
    { throw std::runtime_error("Test exception"); };
    EXPECT_THROW(callable(), std::runtime_error);
}

// Test 8: Callable with multiple arguments
TEST_F(LightCallableTest, MultipleArguments)
{
    stdx::LightCallable<int(int, double)> callable = [](int a, double b)
    {
        return a + static_cast<int>(b);
    };
    EXPECT_EQ(callable(3, 4.5), 7);
}

// Test 9: Callable with void return type
TEST_F(LightCallableTest, VoidReturn)
{
    bool executed = false;
    stdx::LightCallable<void()> callable = [&executed]()
    { executed = true; };
    callable();
    EXPECT_TRUE(executed);
}

// Test 10: Const callable
TEST_F(LightCallableTest, ConstCallable)
{
    struct ConstFunctor
    {
        int operator()(int x) const { return x * 2; }
    };
    stdx::LightCallable<int(int)> callable = ConstFunctor{};
    EXPECT_EQ(callable(5), 10);
}

// Test 11: Callable with reference argument
TEST_F(LightCallableTest, ReferenceArgument)
{
    int value = 0;
    stdx::LightCallable<void(int &)> callable = [](int &ref)
    { ref = 42; };
    callable(value);
    EXPECT_EQ(value, 42);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}