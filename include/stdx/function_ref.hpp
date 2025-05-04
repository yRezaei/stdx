#ifndef STDX_FUNCTION_REF_HPP
#define STDX_FUNCTION_REF_HPP

#include <type_traits>
#include <functional>
#include <utility>

namespace stdx
{

    /**
     * @brief A lightweight non-owning reference to a callable.
     */
    template <typename Signature>
    class FunctionRef;

    template <typename R, typename... Args>
    class FunctionRef<R(Args...)>
    {
    private:
        using InvokerFn = R (*)(void *, Args &&...);

        void *callable_ = nullptr;
        InvokerFn invoker_ = nullptr;

        template <typename F>
        static R invokeImpl(void *callable, Args &&...args)
        {
            return (*static_cast<F *>(callable))(std::forward<Args>(args)...);
        }

        // Special case for function pointers
        template <typename ReturnType, typename... Arguments>
        static R invokeFunctionPtr(void *callable, Args &&...args)
        {
            auto func_ptr = reinterpret_cast<ReturnType (*)(Arguments...)>(callable);
            return (*func_ptr)(std::forward<Args>(args)...);
        }

    public:
        FunctionRef() noexcept = default;
        FunctionRef(std::nullptr_t) noexcept : callable_(nullptr), invoker_(nullptr) {}

        // Constructor for regular callables (not function pointers)
        template <typename F,
                  typename = std::enable_if_t<
                      !std::is_same_v<std::decay_t<F>, FunctionRef> &&
                      !std::is_function_v<std::remove_pointer_t<std::remove_reference_t<F>>> &&
                      std::is_invocable_r_v<R, F &, Args...>>>
        FunctionRef(F &f) noexcept
        {
            callable_ = &f;
            invoker_ = &invokeImpl<F>;
        }

        // Explicit constructor for function pointers
        template <typename ReturnType, typename... Arguments,
                  std::enable_if_t<
                      std::is_same_v<ReturnType, R> &&
                          std::is_same_v<std::tuple<Arguments...>, std::tuple<Args...>>,
                      int> = 0>
        FunctionRef(ReturnType (*f)(Arguments...)) noexcept
        {
            callable_ = reinterpret_cast<void *>(f);
            invoker_ = &invokeFunctionPtr<ReturnType, Arguments...>;
        }

        FunctionRef(const FunctionRef &) noexcept = default;
        FunctionRef &operator=(const FunctionRef &) noexcept = default;

        explicit operator bool() const noexcept
        {
            return callable_ != nullptr;
        }

        void reset() noexcept
        {
            callable_ = nullptr;
            invoker_ = nullptr;
        }

        R operator()(Args... args) const
        {
            if (!callable_)
            {
                throw std::bad_function_call();
            }
            return invoker_(callable_, std::forward<Args>(args)...);
        }

        void swap(FunctionRef &other) noexcept
        {
            std::swap(callable_, other.callable_);
            std::swap(invoker_, other.invoker_);
        }
    };

    // Deduction guide for function pointers
    template <typename R, typename... Args>
    FunctionRef(R (*)(Args...)) -> FunctionRef<R(Args...)>;

    template <typename Signature>
    void swap(FunctionRef<Signature> &lhs, FunctionRef<Signature> &rhs) noexcept
    {
        lhs.swap(rhs);
    }

} // namespace stdx

#endif // STDX_FUNCTION_REF_HPP