#ifndef A463C1C8_333D_4A72_8D97_4A121B22765A
#define A463C1C8_333D_4A72_8D97_4A121B22765A

#include <array>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <functional>

namespace stdx
{
    // Forward declaration
    template <typename Signature>
    class LightCallable;

    // Specialization for function signature R(Args...)
    template <typename R, typename... Args>
    class LightCallable<R(Args...)>
    {
    private:
        // Configuration
        static constexpr size_t BUFFER_SIZE = 32;                         // Size of the inline buffer
        static constexpr size_t BUFFER_ALIGN = alignof(std::max_align_t); // Maximum alignment

        // VTable structure for type erasure
        struct VTable
        {
            R (*invoke_fn)(void *, Args...);       // Invoke the callable
            void (*destroy_fn)(void *);            // Destroy the callable
            void (*copy_fn)(const void *, void *); // Copy the callable
        };

        // Helper struct for inline storage operations
        template <typename Callable>
        struct VTableFor
        {
            static const VTable vtable;

            static R invoke(void *ptr, Args... args)
            {
                return (*static_cast<Callable *>(ptr))(std::forward<Args>(args)...);
            }
            static void destroy(void *ptr)
            {
                static_cast<Callable *>(ptr)->~Callable();
            }
            static void copy(const void *src, void *dst)
            {
                new (dst) Callable(*static_cast<const Callable *>(src));
            }
        };

        // Helper struct for heap storage operations
        template <typename Callable>
        struct VTableForShared
        {
            static const VTable vtable;

            static R invoke(void *ptr, Args... args)
            {
                auto &sp = *static_cast<std::shared_ptr<Callable> *>(ptr);
                return (*sp)(std::forward<Args>(args)...);
            }
            static void destroy(void *ptr)
            {
                static_cast<std::shared_ptr<Callable> *>(ptr)->~shared_ptr();
            }
            static void copy(const void *src, void *dst)
            {
                const auto &src_sp = *static_cast<const std::shared_ptr<Callable> *>(src);
                new (dst) std::shared_ptr<Callable>(src_sp);
            }
        };

        // Storage for the callable (either inline or a shared_ptr)
        alignas(BUFFER_ALIGN) std::array<char, BUFFER_SIZE> storage_;
        void *data_;           // Pointer to the callable (either in storage_ or heap)
        const VTable *vtable_; // Pointer to the vtable
        bool is_inline_;       // Flag indicating inline vs. heap storage

        // Helper functions to access storage
        void *get_storage() noexcept { return storage_.data(); }
        const void *get_storage() const noexcept { return storage_.data(); }

        // Determine if the callable can be stored inline
        template <typename T>
        static constexpr bool is_inline_v =
            sizeof(T) <= BUFFER_SIZE &&
            alignof(T) <= BUFFER_ALIGN &&
            std::is_nothrow_move_constructible_v<T> &&
            std::is_copy_constructible_v<T>;

    public:
        // **Default Constructor**
        // Initializes an empty LightCallable
        LightCallable() noexcept
            : data_(nullptr), vtable_(nullptr), is_inline_(true) {}

        // **Constructor from Callable (Inline Storage)**
        // For small, nothrow move-constructible callables
        template <typename F, typename DecayedF = std::decay_t<F>>
        LightCallable(F &&f, std::enable_if_t<is_inline_v<DecayedF>, int> = 0)
            : is_inline_(true), vtable_(&VTableFor<DecayedF>::vtable)
        {
            new (get_storage()) DecayedF(std::forward<F>(f));
            data_ = get_storage();
        }

        // **Constructor from Callable (Heap Storage)**
        // For larger or non-nothrow move-constructible callables
        template <typename F,
                  typename DecayedF = std::decay_t<F>,
                  // Exclude the case that DecayedF is exactly the same LightCallable type.
                  typename = std::enable_if_t<!std::is_same_v<DecayedF, LightCallable<R(Args...)>>>>
        LightCallable(F &&f, std::enable_if_t<!is_inline_v<DecayedF>, int> = 0)
            : is_inline_(false), vtable_(&VTableForShared<DecayedF>::vtable)
        {
            using SharedPtr = std::shared_ptr<DecayedF>;
            new (get_storage()) SharedPtr(std::make_shared<DecayedF>(std::forward<F>(f)));
            data_ = get_storage();
        }

        // **Copy Constructor**
        // Creates a deep copy of the callable
        LightCallable(const LightCallable &other) : is_inline_(other.is_inline_)
        {
            if (!other.vtable_)
            {
                vtable_ = nullptr;
                data_ = nullptr;
                return;
            }

            void *storage = get_storage();
            other.vtable_->copy_fn(other.data_, storage);
            data_ = storage; // Set data_ after copying
            vtable_ = other.vtable_;
        }

        // **Move Constructor**
        // Transfers ownership of the callable
        LightCallable(LightCallable &&other) noexcept : is_inline_(other.is_inline_)
        {
            if (!other.vtable_)
            {
                vtable_ = nullptr;
                data_ = nullptr;
                return;
            }

            void *storage = get_storage();

            if (is_inline_)
            {
                // For inline storage, we need a real copy - can't safely move from inline storage
                other.vtable_->copy_fn(other.data_, storage);
            }
            else
            {
                // For heap storage, we can move the shared_ptr
                new (storage) std::shared_ptr<void>(
                    std::move(*static_cast<std::shared_ptr<void> *>(other.data_)));
            }

            data_ = storage;
            vtable_ = other.vtable_;

            // For heap storage, we can safely null out the source
            other.vtable_ = nullptr;
            other.data_ = nullptr;

        }

        // **Destructor**
        // Cleans up the stored callable
        ~LightCallable()
        {
            if (vtable_)
            {
                vtable_->destroy_fn(data_);
            }
        }

        // **Copy Assignment Operator**
        LightCallable &operator=(const LightCallable &other)
        {
            if (this != &other)
            {
                if (vtable_)
                {
                    vtable_->destroy_fn(data_);
                }
                is_inline_ = other.is_inline_;
                if (other.vtable_)
                {
                    other.vtable_->copy_fn(other.data_, get_storage());
                    data_ = get_storage();
                    vtable_ = other.vtable_;
                }
                else
                {
                    vtable_ = nullptr;
                    data_ = nullptr;
                }
            }
            return *this;
        }

        // **Move Assignment Operator**
        LightCallable &operator=(LightCallable &&other) noexcept
        {
            if (this != &other)
            {
                if (vtable_)
                {
                    vtable_->destroy_fn(data_);
                }

                is_inline_ = other.is_inline_;

                if (other.vtable_)
                {
                    if (is_inline_)
                    {
                        // For inline storage, we need a real copy
                        other.vtable_->copy_fn(other.data_, get_storage());
                        // Do NOT destroy the source for inline objects
                    }
                    else
                    {
                        // For heap storage, move the shared_ptr
                        new (get_storage()) std::shared_ptr<void>(
                            std::move(*static_cast<std::shared_ptr<void> *>(other.data_)));
                        data_ = get_storage();
                        other.vtable_->destroy_fn(other.data_);
                    }

                    vtable_ = other.vtable_;
                    data_ = get_storage();


                    other.vtable_ = nullptr;
                    other.data_ = nullptr;
                }
                else
                {
                    vtable_ = nullptr;
                    data_ = nullptr;
                }
            }
            return *this;
        }

        // **Invocation Operator**
        // Calls the stored callable with the provided arguments
        R operator()(Args... args)
        {
            if (!vtable_)
            {
                throw std::bad_function_call();
            }
            return vtable_->invoke_fn(data_, std::forward<Args>(args)...);
        }

        // **Validity Check**
        // Returns true if the LightCallable holds a valid callable
        explicit operator bool() const noexcept
        {
            return vtable_ != nullptr;
        }
    };

    template <typename R, typename... Args>
    template <typename Callable>
    const typename LightCallable<R(Args...)>::VTable LightCallable<R(Args...)>::VTableFor<Callable>::vtable = {
        &VTableFor<Callable>::invoke,
        &VTableFor<Callable>::destroy,
        &VTableFor<Callable>::copy};

    template <typename R, typename... Args>
    template <typename Callable>
    const typename LightCallable<R(Args...)>::VTable LightCallable<R(Args...)>::VTableForShared<Callable>::vtable = {
        &VTableForShared<Callable>::invoke,
        &VTableForShared<Callable>::destroy,
        &VTableForShared<Callable>::copy};
}

#endif /* A463C1C8_333D_4A72_8D97_4A121B22765A */
