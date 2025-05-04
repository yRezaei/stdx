#ifndef B66A22ED_DA29_4230_89AE_0D929735F32A
#define B66A22ED_DA29_4230_89AE_0D929735F32A

#include <cstring>
#include <array>
#include <type_traits>
#include <utility>
#include <new>
#include <functional>
#include <exception>

namespace stdx
{

    /**
     * @brief A small-function wrapper templated by signature R(Args...).
     * Move-only, optimized for minimal footprint with enhanced performance.
     * Added manual clone() method for explicit copying when needed.
     */
    template <typename Signature>
    class InlineFunction;

    template <typename R, typename... Args>
    class InlineFunction<R(Args...)>
    {
    private:
        // Reduce storage size for better cache locality
        static constexpr size_t STORAGE_SIZE = 64;
        static constexpr size_t STORAGE_ALIGN = alignof(std::max_align_t);

        // Use struct for operations to reduce pointer count and add clone operation
        struct VTable
        {
            R (*invoke)(void *, Args &&...);
            void (*destroy)(void *);
            void (*clone)(const void *, void *); // New clone operation for manual copying
        };

        alignas(STORAGE_ALIGN) std::array<char, STORAGE_SIZE> storage_ = {};
        const VTable *vtable_ = nullptr;
        bool is_inline_ = true;

        void *get_storage() noexcept { return static_cast<void *>(storage_.data()); }
        const void *get_storage() const noexcept { return static_cast<const void *>(storage_.data()); }

    public:
        // default ctor => empty
        InlineFunction() noexcept = default;

        // explicitly delete copy operations
        InlineFunction(const InlineFunction &) = delete;
        InlineFunction &operator=(const InlineFunction &) = delete;

        // optimized move operations
        InlineFunction(InlineFunction &&other) noexcept
            : vtable_(other.vtable_), is_inline_(other.is_inline_)
        {
            if (vtable_)
            {
                if (is_inline_)
                {
                    std::memcpy(storage_.data(), other.storage_.data(), STORAGE_SIZE);
                }
                else
                {
                    std::memcpy(storage_.data(), other.storage_.data(), sizeof(void *));
                }
                other.vtable_ = nullptr;
            }
        }

        InlineFunction &operator=(InlineFunction &&other) noexcept
        {
            if (this != &other)
            {
                clear();
                vtable_ = other.vtable_;
                is_inline_ = other.is_inline_;
                if (vtable_)
                {
                    if (is_inline_)
                    {
                        std::memcpy(storage_.data(), other.storage_.data(), STORAGE_SIZE);
                    }
                    else
                    {
                        std::memcpy(storage_.data(), other.storage_.data(), sizeof(void *));
                    }
                    other.vtable_ = nullptr;
                }
            }
            return *this;
        }

        // construct from any callable "F"
        template <typename F,
                  typename = std::enable_if_t<
                      !std::is_same_v<std::remove_cvref_t<F>, InlineFunction> &&
                      std::is_invocable_r_v<R, F &, Args...>>>
        InlineFunction(F &&f) noexcept(std::is_nothrow_move_constructible_v<std::remove_reference_t<F>>)
        {
            using DecayedF = std::remove_cvref_t<F>;

            if constexpr (sizeof(DecayedF) <= STORAGE_SIZE &&
                          std::is_nothrow_move_constructible_v<DecayedF> &&
                          alignof(DecayedF) <= STORAGE_ALIGN)
            {
                // store inline
                is_inline_ = true;
                new (get_storage()) DecayedF(std::forward<F>(f));

                static const VTable vtable = {
                    [](void *ptr, Args &&...args) -> R
                    {
                        return (*static_cast<DecayedF *>(ptr))(std::forward<Args>(args)...);
                    },
                    [](void *ptr)
                    {
                        static_cast<DecayedF *>(ptr)->~DecayedF();
                    },
                    [](const void *src, void *dst)
                    {
                        // Clone by copy-constructing from source
                        new (dst) DecayedF(*static_cast<const DecayedF *>(src));
                    }};
                vtable_ = &vtable;
            }
            else
            {
                // allocate on heap
                is_inline_ = false;
                auto *heap_ptr = new DecayedF(std::forward<F>(f));
                std::memcpy(storage_.data(), &heap_ptr, sizeof(heap_ptr));

                static const VTable vtable = {
                    [](void *s, Args &&...args) -> R
                    {
                        DecayedF *func_ptr;
                        std::memcpy(&func_ptr, s, sizeof(func_ptr));
                        return (*func_ptr)(std::forward<Args>(args)...);
                    },
                    [](void *s)
                    {
                        DecayedF *func_ptr;
                        std::memcpy(&func_ptr, s, sizeof(func_ptr));
                        delete func_ptr;
                    },
                    [](const void *src, void *dst)
                    {
                        // Clone by deep-copying heap-allocated object
                        DecayedF *src_ptr;
                        std::memcpy(&src_ptr, src, sizeof(src_ptr));
                        auto *new_ptr = new DecayedF(*src_ptr);
                        std::memcpy(dst, &new_ptr, sizeof(new_ptr));
                    }};
                vtable_ = &vtable;
            }
        }

        ~InlineFunction()
        {
            clear();
        }

        // check if valid
        explicit operator bool() const noexcept
        {
            return vtable_ != nullptr;
        }

        // invoke - add const version
        R operator()(Args... args) const
        {
            if (!vtable_)
            {
                throw std::bad_function_call();
            }
            return vtable_->invoke(const_cast<void *>(get_storage()), std::forward<Args>(args)...);
        }

        void clear() noexcept
        {
            if (vtable_)
            {
                vtable_->destroy(get_storage());
                vtable_ = nullptr;
            }
        }

        /**
         * @brief Creates an explicit copy of this function object.
         *
         * This provides manual copying capability while maintaining the move-only
         * semantics of InlineFunction. Use when you need to store a copy.
         *
         * @return InlineFunction A new function object that's a copy of this one
         */
        InlineFunction clone() const
        {
            if (!vtable_)
            {
                return InlineFunction(); // Return empty function if this is empty
            }

            InlineFunction result;
            result.vtable_ = vtable_;
            result.is_inline_ = is_inline_;

            // Use the vtable clone function to create a proper copy
            vtable_->clone(get_storage(), result.get_storage());

            return result;
        }

        // Add swap for efficient operations
        void swap(InlineFunction &other) noexcept
        {
            if (this == &other)
                return;

            InlineFunction temp(std::move(*this));
            *this = std::move(other);
            other = std::move(temp);
        }
    };

    template <typename F>
    InlineFunction(F &&) -> InlineFunction<std::invoke_result_t<F &>()>;

    template <typename F, typename Ret = std::invoke_result_t<F &>>
    InlineFunction(F &&) -> InlineFunction<Ret()>;

    template <typename R, typename... Args, typename F>
    InlineFunction<R(Args...)> make_function(F &&f)
    {
        return InlineFunction<R(Args...)>(std::forward<F>(f));
    }

    // Free function for ADL-based swap
    template <typename R, typename... Args>
    void swap(InlineFunction<R(Args...)> &lhs, InlineFunction<R(Args...)> &rhs) noexcept
    {
        lhs.swap(rhs);
    }

} // namespace stdx

#endif /* B66A22ED_DA29_4230_89AE_0D929735F32A */