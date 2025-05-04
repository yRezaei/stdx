#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

namespace stdx
{

    /**
     * @brief A simple singleton base class using a static std::unique_ptr.
     *
     * - The singleton instance remains valid until you call `destroy()`,
     *   after which point `instance()` or `create(...)` will create a new one if asked.
     * - `create(...)` constructs the instance if not already present, ignoring new arguments otherwise.
     * - `instance()` returns a reference to the object, or throws if it doesn't exist.
     * - `destroy()` resets the unique_ptr, destroying the object immediately.
     *
     * Usage example:
     * @code
     *   class MyService : public Singleton<MyService> {
     *       friend class Singleton<MyService>;
     *   private:
     *       MyService(int x) : x_(x) {}
     *       int x_;
     *   public:
     *       void doSomething() {...}
     *   };
     *
     *   // Create the singleton
     *   MyService& s1 = MyService::create(42);
     *   MyService& s2 = MyService::instance(); // same object, x=42
     *   // If you call MyService::destroy(), s1 and s2 references are invalid.
     * @endcode
     */
    template <typename Derived>
    class Singleton
    {
    public:
        /**
         * @brief Creates the singleton if it doesn't exist, or returns the existing one.
         * @tparam Args Constructor argument types
         * @param args Constructor arguments for Derived
         * @return A reference to the singleton instance
         *
         * If the singleton already exists, new arguments are ignored.
         */
        template <typename... Args>
        static Derived &create(Args &&...args)
        {
            if (!instance_ptr_)
            {
                // Use a temporary unique_ptr and the protected create_instance method
                instance_ptr_ = create_instance(std::forward<Args>(args)...);
            }
            return *instance_ptr_;
        }

        /**
         * @brief Access the singleton instance by reference.
         * @throws std::runtime_error if none exists (i.e., not created or already destroyed).
         */
        static Derived &instance()
        {
            if (!instance_ptr_)
            {
                throw std::runtime_error(
                    std::string("You are trying to access the instance of ") +
                    typeid(Derived).name() +
                    " that has not been created yet (or was destroyed)!");
            }
            return *instance_ptr_;
        }

        /**
         * @brief Destroy the singleton by resetting the unique_ptr.
         *
         * Any existing references to `instance()` become invalid as soon as `destroy()` is called.
         * Use carefully to avoid dangling references.
         */
        static void destroy()
        {
            instance_ptr_.reset();
        }

    protected:
        Singleton() = default;
        virtual ~Singleton() = default;

        // Non-copyable, non-assignable
        Singleton(const Singleton &) = delete;
        Singleton &operator=(const Singleton &) = delete;

        // Factory method that derived classes can access as friends
        template <typename... Args>
        static std::unique_ptr<Derived> create_instance(Args &&...args)
        {
            return std::unique_ptr<Derived>(new Derived(std::forward<Args>(args)...));
        }

        // The one static unique_ptr to manage the Derived object.
        inline static std::unique_ptr<Derived> instance_ptr_{};
    };

}