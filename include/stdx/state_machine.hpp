#ifndef CC2BA826_A94F_4343_8ADD_B56F093DCB9F
#define CC2BA826_A94F_4343_8ADD_B56F093DCB9F

#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <future>
#include <mutex>
#include <atomic>
#include "stdx/threading/thread_pool.hpp"
#include "stdx/light_callable.hpp"

namespace stdx
{
    template <typename T, typename = void>
    struct has_num_of_states : std::false_type
    {
    };

    template <typename T>
    struct has_num_of_states<T, std::void_t<decltype(T::NUM_OF_STATES)>> : std::true_type
    {
    };

    /// A generic state machine that supports both event-driven and direct transitions
    template <typename StateEnum, typename EventEnum = int>
    class StateMachine
    {
    private:
        struct PairHash
        {
            std::size_t operator()(const std::pair<StateEnum, EventEnum> &p) const
            {
                auto h1 = std::hash<StateEnum>{}(p.first);
                auto h2 = std::hash<EventEnum>{}(p.second);
                return h1 ^ (h2 << 1);
            }
        };

    public:
        using state_callback = stdx::LightCallable<StateEnum()>;
        using transition_pair = std::pair<StateEnum, EventEnum>;
        using event_transition_map = std::unordered_map<transition_pair, StateEnum, PairHash>;
        using direct_transition_map = std::unordered_map<StateEnum, StateEnum>;

    private:
        std::atomic<StateEnum> current_state_;
        StateEnum default_state_;
        StateEnum terminal_state_;
        std::unordered_map<StateEnum, state_callback> state_callbacks_;
        direct_transition_map direct_transitions_;
        event_transition_map event_transitions_;
        stdx::threading::ThreadPool &thread_pool_;
        std::atomic<bool> running_{false};
        std::mutex transition_mutex_;
        stdx::LightCallable<void(std::exception_ptr)> exception_handler_;

    public:
        explicit StateMachine(stdx::threading::ThreadPool &thread_pool, StateEnum default_state, StateEnum terminal_state)
            : thread_pool_(thread_pool), default_state_(default_state), terminal_state_(terminal_state), current_state_(default_state)
        {
            static_assert(has_num_of_states<StateEnum>::value, "StateEnum must define NUM_OF_STATES");
        }

        void register_state(StateEnum state, state_callback callback) noexcept
        {
            state_callbacks_[state] = std::move(callback);
        }

        void add_transition(StateEnum from, StateEnum to) noexcept
        {
            direct_transitions_[from] = to;
        }

        void add_transition(StateEnum from, EventEnum event, StateEnum to) noexcept
        {
            event_transitions_[{from, event}] = to;
        }

        void set_default_state(StateEnum state) noexcept
        {
            default_state_ = state;
        }

        void start(StateEnum initial_state)
        {
            if (running_.exchange(true))
            {
                return;
            }
            execute_state(initial_state);
        }

        void stop() noexcept
        {
            running_.store(false);
        }

        StateEnum get_current_state() const noexcept
        {
            return current_state_.load();
        }

        void trigger_event(EventEnum event)
        {
            if (!running_.load())
            {
                return;
            }

            std::lock_guard<std::mutex> lock(transition_mutex_);
            StateEnum current = current_state_.load();

            auto key = std::make_pair(current, event);
            auto it = event_transitions_.find(key);
            if (it != event_transitions_.end())
            {
                StateEnum new_state = it->second;
                if (state_callbacks_.find(new_state) != state_callbacks_.end())
                {
                    current_state_.store(new_state);
                    execute_state(new_state);
                }
                else
                {
                    handle_callback_missing(new_state);
                }
            }
        }

        void set_exception_handler(stdx::LightCallable<void(std::exception_ptr)> handler)
        {
            exception_handler_ = std::move(handler);
            if (exception_handler_)
            {
                thread_pool_.set_exception_handler(exception_handler_);
            }
        }

    private:
        void execute_state(StateEnum state)
        {
            auto it = state_callbacks_.find(state);
            if (it == state_callbacks_.end())
            {
                handle_callback_missing(state);
                return;
            }

            // Submit asynchronously without waiting
            thread_pool_.submit(0, [this, state, callback = it->second]() mutable
                                {
                                    try {
                                        StateEnum next_state = callback();
                                        if (running_.load()) {
                                            handle_direct_transition(state, next_state);
                                        }
                                    } catch (...) {
                                        if (exception_handler_) {
                                            exception_handler_(std::current_exception());
                                        }
                                    } });
        }

        void handle_direct_transition(StateEnum from_state, StateEnum next_state)
        {
            std::lock_guard<std::mutex> lock(transition_mutex_);

            if (current_state_.load() != from_state)
            {
                return;
            }

            // Check if next state is a terminal state
            if (terminal_state_ == next_state) {
                current_state_.store(next_state);
                // Execute terminal state once but don't continue chain
                auto it = state_callbacks_.find(next_state);
                if (it != state_callbacks_.end()) {
                    auto callback_copy = it->second;
                    thread_pool_.submit(0, [callback = std::move(callback_copy)]() mutable {
                        callback();
                    });
                }
                return;
            }

            auto it = direct_transitions_.find(next_state);
            if (it != direct_transitions_.end())
            {
                StateEnum target_state = it->second;
                if (state_callbacks_.find(target_state) != state_callbacks_.end())
                {
                    current_state_.store(target_state);
                    execute_state(target_state);
                }
                else
                {
                    handle_callback_missing(target_state);
                }
            }
            else if (state_callbacks_.find(next_state) != state_callbacks_.end())
            {
                current_state_.store(next_state);
                execute_state(next_state);
            }
            else
            {
                handle_callback_missing(default_state_);
                current_state_.store(default_state_);
                execute_state(default_state_);
            }
        }

        void handle_callback_missing(StateEnum state)
        {
            if (exception_handler_)
            {
                exception_handler_(std::make_exception_ptr(
                    std::runtime_error("Callback not registered for state: " + std::to_string(static_cast<int>(state)))));
            }
        }
    };
}

#endif /* CC2BA826_A94F_4343_8ADD_B56F093DCB9F */
