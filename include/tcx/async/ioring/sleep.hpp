#ifndef TCX_ASYNC_IORING_SLEEP_HPP
#define TCX_ASYNC_IORING_SLEEP_HPP

#include <chrono>
#include <memory>

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

namespace impl {

    struct ioring_timeout_operation {
        using result_type = void;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, std::unique_ptr<__kernel_timespec> spec, int flags, F &&f)
        {
            service.async_timeout(spec.get(), flags, [spec = std::move(spec), &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                spec.reset();
                executor.post([f = std::move(f), result]() mutable {
                    if (result < 0)
                        f(std::error_code { -result, std::system_category() });
                    else
                        f(std::error_code {});
                });
            });
        }
    };
}

template <typename E, typename F, typename Rep, typename Ratio>
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation::result_type>
auto async_sleep_for(E &executor, tcx::ioring_service &service, std::chrono::duration<Rep, Ratio> duration, F &&f)
{
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(duration);
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(duration - secs);

    auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

    return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation>::call(executor, service, std::forward<F>(f), 0, std::move(spec));
}

template <typename E, typename F, typename Dur>
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation::result_type>
auto async_sleep_until(E &executor, tcx::ioring_service &service, std::chrono::time_point<std::chrono::steady_clock, Dur> time, F &&f)
{
    // io_uring uses CLOCK_MONOTONIC by default, which is what std::chrono::steady_clock uses
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(time.time_since_epoch());
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(time.time_since_epoch() - secs);

    auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

    return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation>::call(executor, service, std::forward<F>(f), IORING_TIMEOUT_ABS, std::move(spec));
}

template <typename E, typename F, typename Dur>
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation::result_type>
auto async_sleep_until(E &executor, tcx::ioring_service &service, std::chrono::time_point<std::chrono::system_clock, Dur> time, F &&f)
{
    // std::chrono::system_clock uses CLOCK_REALTIME
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(time.time_since_epoch());
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(time.time_since_epoch() - secs);

    auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

    return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation>::call(executor, service, std::forward<F>(f), IORING_TIMEOUT_ABS | IORING_TIMEOUT_REALTIME, std::move(spec));
}

}

#endif