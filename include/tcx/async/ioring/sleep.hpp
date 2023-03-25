#ifndef TCX_ASYNC_IORING_SLEEP_HPP
#define TCX_ASYNC_IORING_SLEEP_HPP

#include <chrono>
#include <memory>

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/services/uring_service.hpp>

namespace tcx {

namespace impl {

    struct ioring_timeout_operation {
        using result_type = void;

        template <typename E, typename F>
        static auto call(E &executor, tcx::uring_context auto &service, std::unique_ptr<__kernel_timespec> spec, int flags, F &&f)
        {
            return service.async_timeout(spec.get(), flags, [spec = std::move(spec), &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                spec.reset();
                return executor.post([f = std::move(f), result]() mutable {
                    if (result < 0)
                        return f(std::error_code { -result, std::system_category() });
                    else
                        return f(std::error_code {});
                });
            });
        }
    };
} // namespace impl

/**
 * @ingroup ioring_service
 * @param executor
 * @param service
 * @param duration relative to std::chrono::steady_clock
 * @param f completion
 */
template <typename E, typename F, typename Rep, typename Ratio>
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation::result_type>
auto async_sleep_for(E &executor, tcx::uring_context auto &service, std::chrono::duration<Rep, Ratio> duration, F &&f)
{
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(duration);
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(duration - secs);

    auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

    return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation>::call(executor, service, std::forward<F>(f), 0, std::move(spec));
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F, typename Dur>
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation::result_type>
auto async_sleep_until(E &executor, tcx::uring_context auto &service, std::chrono::time_point<std::chrono::steady_clock, Dur> time, F &&f)
{
    // io_uring uses CLOCK_MONOTONIC by default, which is what std::chrono::steady_clock uses
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(time.time_since_epoch());
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(time.time_since_epoch() - secs);

    auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

    return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation>::call(executor, service, std::forward<F>(f), IORING_TIMEOUT_ABS, std::move(spec));
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F, typename Dur>
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation::result_type>
auto async_sleep_until(E &executor, tcx::uring_context auto &service, std::chrono::time_point<std::chrono::system_clock, Dur> time, F &&f)
{
    // std::chrono::system_clock uses CLOCK_REALTIME
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(time.time_since_epoch());
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(time.time_since_epoch() - secs);

    auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

    return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation>::call(executor, service, std::forward<F>(f), IORING_TIMEOUT_ABS | IORING_TIMEOUT_REALTIME, std::move(spec));
}

// there's no standard clock for CLOCK_BOOTTIME

} // namespace tcx

#endif
