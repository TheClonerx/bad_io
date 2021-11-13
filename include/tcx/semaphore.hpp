#ifndef TCX_SEMAPHORE_HPP
#define TCX_SEMAPHORE_HPP

#include <atomic> // std::atomic_ptrdiff_t
#include <cassert>
#include <semaphore> // std::counting_semaphore<>::max()
#include <type_traits> // std::is_invokable_v
#include <utility> // std::forward

#include <moodycamel/concurrentqueue.h>

namespace tcx {

template <typename FunctionStorage, typename E, std::ptrdiff_t LeastMaxValue = PTRDIFF_MAX>
class basic_semaphore {
private:
public:
    using executor_type = E;
    using function_storage = FunctionStorage;

    constexpr explicit basic_semaphore(E &executor, std::ptrdiff_t desired = LeastMaxValue)
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
        return LeastMaxValue;
    }

private:
    std::atomic_ptrdiff_t m_count;
    E &m_executor;
    moodycamel::ConcurrentQueue<function_storage> m_functions;
};

#include <tcx/unique_function.hpp>

template <typename E, std::ptrdiff_t LeastMaxValue = PTRDIFF_MAX>
struct semaphore : basic_semaphore<tcx::unique_function<void()>, E, LeastMaxValue> {
    using basic_semaphore<tcx::unique_function<void()>, E, LeastMaxValue>::basic_semaphore;
};

template <typename E>
semaphore(E &, std::ptrdiff_t) -> semaphore<E, PTRDIFF_MAX>;

template <typename E>
using binary_semaphore = semaphore<E, 1>;

}

#endif