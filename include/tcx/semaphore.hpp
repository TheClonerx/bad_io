#ifndef TCX_SEMAPHORE_HPP
#define TCX_SEMAPHORE_HPP

#include <atomic> // std::atomic_ptrdiff_t
#include <cassert>
#include <semaphore> // std::counting_semaphore<>::max()
#include <type_traits> // std::is_invokable_v
#include <utility> // std::forward

#include <moodycamel/concurrentqueue.h>

namespace tcx {

template <std::ptrdiff_t Max, typename E, typename FunctionStorage>
class basic_semaphore {
private:
public:
    using executor_type = E;
    using function_storage = FunctionStorage;

    constexpr explicit basic_semaphore(E &executor, std::ptrdiff_t desired = Max)
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
        if (!async_try_acquire(std::forward<F>(f))) {
            m_functions.enqueue(function_storage(std::forward<F>(f)));
        }
    }

    template <typename F>
    bool async_try_acquire(F &&f) requires std::is_invocable_v<F>
    {
        if (auto old = m_count.fetch_sub(1); old - 1 > 0) {
            executor().post(std::forward<F>(f));
            return true;
        } else {
            return false;
        }
    }

    void release(std::ptrdiff_t update = 1)
    {
        if (auto old = m_count.fetch_add(update); old + update > 0) {
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
        return Max;
    }

private:
    std::atomic_ptrdiff_t m_count;
    E &m_executor;
    moodycamel::ConcurrentQueue<function_storage> m_functions;
};

#include <tcx/unique_function.hpp>

template <std::ptrdiff_t Max, typename E>
struct semaphore : basic_semaphore<Max, E, tcx::unique_function<void()>> {
    using basic_semaphore<Max, E, tcx::unique_function<void()>>::basic_semaphore;
};

template <typename E>
semaphore(E &, std::ptrdiff_t) -> semaphore<std::counting_semaphore<>::max(), E>;
}

#endif