#ifndef TCX_ASYNC_IORING_SLEEP_HPP
#define TCX_ASYNC_IORING_SLEEP_HPP

#include <chrono>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

template <typename E, typename F, typename Rep, typename Ratio>
void async_sleep_for(E &executor, tcx::ioring_service &service, std::chrono::duration<Rep, Ratio> duration, F &&f) requires std::is_invocable_v<F, std::error_code>
{
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(duration);
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(duration - secs);

    __kernel_timespec const spec { secs.count(), nsecs.count() };

    service.async_timeout(&spec, false, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
        if (result < 0)
            executor.post([f = std::move(f), result]() mutable {
                f(std::error_code { -result, std::system_category() });
            });
        else
            executor.post([f = std::move(f), result]() mutable {
                f(std::error_code {});
            });
    });
}

template <typename E, typename F, typename Clock, typename Dur>
void async_timeout_until(E &executor, tcx::ioring_service &service, std::chrono::time_point<Clock, Dur> time, F &&f) requires(std::is_invocable_v<F, std::error_code>)
{
    if constexpr (std::is_same_v<Clock, std::chrono::steady_clock>) { // io_uring uses CLOCK_MONOTONIC, which is what std::chrono::steady_clock uses
        using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
        using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

        auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(time.time_since_epoch());
        auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(time.time_since_epoch() - secs);
        __kernel_timespec const spec { secs.count(), nsecs.count() };

        service.async_timeout(0, &spec, true, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
            if (result < 0)
                executor.post([f = std::move(f), result]() mutable {
                    f(std::error_code { -result, std::system_category() });
                });
            else
                executor.post([f = std::move(f), result]() mutable {
                    f(std::error_code {});
                });
        });
    } else {
        auto const duration = time - Clock::now();
        tcx::async_sleep_for(executor, duration, std::forward<F>());
    }
}

}

#endif