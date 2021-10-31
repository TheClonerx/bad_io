#ifndef TCX_SEMAPHORE_HPP
#define TCX_SEMAPHORE_HPP

#include <atomic> // std::atomic_ptrdiff_t
#include <cassert>
#include <coroutine>
#include <semaphore> // std::counting_semaphore<>::max()
#include <type_traits> // std::is_invokable_v
#include <utility> // std::forward

#include <moodycamel/concurrentqueue.h>

#include <tcx/async/use_awaitable.hpp>
#include <tcx/unique_function.hpp>

namespace tcx {

template <std::ptrdiff_t Max, typename E>
class semaphore {
private:
public:
    using executor_type = E;
    using function_storage = tcx::unique_function<void()>;

    constexpr explicit semaphore(E &executor, std::ptrdiff_t desired = Max)
        : m_count(desired)
        , m_executor(executor)
    {
    }

    executor_type &executor() const noexcept
    {
        return m_executor;
    }

    auto async_acquire(tcx::use_awaitable_t)
    {
        struct awaiter {
            semaphore &self;

            constexpr bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<> h)
            {
                // might resume us right away
                self.acquire([h = std::move(h)]() {
                    h.resume();
                });
            }

            constexpr void await_resume() const noexcept { }
        };
        return awaiter { *this };
    }

    template <typename F>
    void async_acquire(F &&f) requires std::is_invocable_v<F>
    {
        if (auto old = m_count.fetch_sub(1); old - 1 > 0) {
            executor().post(std::forward<F>(f));
        } else {
            m_functions.enqueue(function_storage(std::forward<F>(f)));
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

template <typename E>
semaphore(E &, std::ptrdiff_t) -> semaphore<std::counting_semaphore<>::max(), E>;

}

#endif