#ifndef TCX_ASYNC_LOCKS_ACQUIRE_HPP
#define TCX_ASYNC_LOCKS_ACQUIRE_HPP

#include <system_error>
#include <tcx/async/concepts.hpp>
#include <tcx/async/locks/guard.hpp>
#include <tcx/async/wrap_op.hpp>
#include <utility>

namespace tcx {

namespace impl {

    template <typename S>
    struct sempahore_acquire_operation {
        using result_type = tcx::unique_lock<S>;

        template <typename E, typename F>
        static void call(E &executor, S &semaphore, F &&f)
        {
            semaphore.async_acquire([f = std::forward<F>(f), &semaphore]() mutable {
                f(std::error_code {} /* async_acquire can't fail... which is odd */, tcx::unique_lock<S> { semaphore, tcx::adopt_lock });
            });
        }
    };

}

template <typename S, typename F>
requires tcx::completion_handler<F, tcx::unique_lock<S>>
auto async_acquire(S &sem, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::sempahore_acquire_operation<S>>::call(sem.executor(), sem, std::forward<F>(f));
}

}

#endif