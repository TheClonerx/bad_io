#ifndef TCX_SEMAPHORE_HPP
#define TCX_SEMAPHORE_HPP

#include <atomic> // std::atomic_ptrdiff_t
#include <cassert>
#include <limits> // std::numeric_limits
#include <type_traits> // std::is_invokable_v
#include <utility> // std::forward

#include <moodycamel/concurrentqueue.h>

namespace tcx {

/**
 * @brief Semaphore that models a non-negative resource count.
 *

 * A basic_semaphore contains an internal counter initialized by the constructor.
 * This counter is decremented by calls to async_acquire() and try_acquire(),
 * and is incremented by calls to release().

 * When the counter reaches zero, async_acquire() stores the callback in an internal list
 * until the counter is incremented, and then is posted to the executor, but try_acquire()
 * instead immediately returns failure.
 * @tparam FunctionStorage
 * @tparam E
 */
template <typename FunctionStorage, typename E>
class basic_semaphore {
private:
    using value_type = std::ptrdiff_t;

    static_assert(std::is_signed_v<value_type>, "The counter must have a signed type");

public:
    using executor_type = E;
    using function_storage = FunctionStorage;

    /**
     * @brief Constructs a `tcx::basic_semaphore`

     * Constructs a `tcx::basic_semaphore` with the internal counter initialized to `desired`.

     * @param executor An executor to post completion handlers for any asynchronous operations.
     * @param desired  The value to initialize `tcx::basic_semaphore`'s counter with.
     */
    basic_semaphore(E &executor, std::ptrdiff_t desired)
        : m_count(desired)
        , m_executor(executor)
    {
    }

    /**
     * @brief Obtain a reference to the executor associated with the object.
     */
    executor_type &executor() const noexcept
    {
        return m_executor;
    }

    /**
     * @brief Decrements the internal counter and stores the callback if reachs zero.

     * Atomically decrements the internal counter by 1;
     * if it reaches zero then the completion is stored it is greater than ​0.

     * @param f Completion handler.
     */
    template <typename F>
    void async_acquire(F &&f) requires std::is_invocable_v<F>
    {
        if (auto const value = m_count.fetch_sub(1, std::memory_order_acquire) - 1; value >= 0) {
            executor().post(std::forward<F>(f));
        } else {
            m_functions.enqueue(function_storage(std::forward<F>(f)));
        }
    }

    /**
     * @brief Tries to atomically decrement the internal counter by one.

     * If the internal counter is greater than ​zero the counter is decremented and returns `true`, otherwise returns `false`.
     */
    bool try_acquire() noexcept
    {
        std::ptrdiff_t old = m_count.load(std::memory_order_acquire);
        for (;;) {
            if (old <= 0)
                return false;
            if (m_count.compare_exchange_strong(old, old - 1, std::memory_order_acquire, std::memory_order_relaxed))
                return true;
        }
    }

    /**
     * @brief Increments the internal counter and posts acquirers.

     * Atomically increments the internal counter by the value of `update`.
     * Any completions(s) waiting for the counter to be greater than ​0​,
     * will subsequently be posted to the associated executor.

     * @param update the amount to increment the internal counter by
     */
    void release(std::ptrdiff_t update = 1)
    {
        if (auto old = m_count.fetch_add(update, std::memory_order_release); old + update > 0) {
            for (std::ptrdiff_t i = 0; i < -old; ++i) {
                function_storage f;
                bool const could = m_functions.try_dequeue(f);
                assert(could && "Queue appears to be empty??"); // sanity check
                executor().post(std::move(f));
            }
        }
    }

    /**
     * @brief Returns the maximum possible value of the internal counter.
     */
    static constexpr std::ptrdiff_t max() noexcept
    {
        return std::numeric_limits<value_type>::max() / sizeof(function_storage);
    }

private:
    std::atomic<value_type> m_count;
    E &m_executor;
    moodycamel::ConcurrentQueue<function_storage> m_functions;
};

#include <tcx/unique_function.hpp>

/**
 * @brief Convenience type using the default function storage type; allowing for deduction guides.
 * @see tcx::basic_semaphore
 */
template <typename E>
struct semaphore : basic_semaphore<tcx::unique_function<void()>, E> {
    using basic_semaphore<tcx::unique_function<void()>, E>::basic_semaphore;
};

template <typename E>
semaphore(E &, std::ptrdiff_t) -> semaphore<E>;

}

#endif