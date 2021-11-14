#ifndef TCX_SEMAPHORE_HPP
#define TCX_SEMAPHORE_HPP

#include <atomic> // std::atomic_ptrdiff_t
#include <cassert>
#include <limits> // std::numeric_limits
#include <type_traits> // std::is_invokable_v
#include <utility> // std::forward

#include <moodycamel/concurrentqueue.h>

namespace tcx {

template <typename FunctionStorage, typename E>
class basic_semaphore {
private:
    using value_type = std::ptrdiff_t;

    static_assert(std::is_signed_v<value_type>, "The counter must have a signed type");

public:
    using executor_type = E;
    using function_storage = FunctionStorage;

    basic_semaphore(E &executor, std::ptrdiff_t desired)
        : m_count(desired)
        , m_executor(executor)
    {
    }

    executor_type &executor() const noexcept
    {
        return m_executor;
    }

    template <typename F>
    void async_acquire(F &&f) requires std::is_invocable_v<F>
    {
        if (auto const value = m_count.fetch_sub(1, std::memory_order_acquire) - 1; value >= 0) {
            executor().post(std::forward<F>(f));
        } else {
            m_functions.enqueue(function_storage(std::forward<F>(f)));
        }
    }

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

template <typename E>
struct semaphore : basic_semaphore<tcx::unique_function<void()>, E> {
    using basic_semaphore<tcx::unique_function<void()>, E>::basic_semaphore;
};

template <typename E>
semaphore(E &, std::ptrdiff_t) -> semaphore<E>;

}

#endif