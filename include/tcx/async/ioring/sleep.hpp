#ifndef TCX_ASYNC_IORING_SLEEP_HPP
#define TCX_ASYNC_IORING_SLEEP_HPP

#include <chrono>
#include <memory>

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

namespace impl {

    template <bool Absolute>
    struct ioring_timeout_operation {
        using result_type = void;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, std::unique_ptr<__kernel_timespec> spec, F &&f)
        {
            service.async_timeout(spec.get(), Absolute, [spec = std::move(spec), &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
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
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation<false>::result_type>
auto async_sleep_for(E &executor, tcx::ioring_service &service, std::chrono::duration<Rep, Ratio> duration, F &&f)
{
    using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
    using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

    auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(duration);
    auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(duration - secs);

    auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

    return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation<false>>::call(executor, service, std::forward<F>(f), std::move(spec));
}

template <typename E, typename F, typename Clock, typename Dur>
requires tcx::completion_handler<F, tcx::impl::ioring_timeout_operation<true>::result_type>
auto async_timeout_until(E &executor, tcx::ioring_service &service, std::chrono::time_point<Clock, Dur> time, F &&f)
{
    if constexpr (std::is_same_v<Clock, std::chrono::steady_clock>) { // io_uring uses CLOCK_MONOTONIC, which is what std::chrono::steady_clock uses
        using sec_t = decltype(std::declval<__kernel_timespec>().tv_sec);
        using nsec_t = decltype(std::declval<__kernel_timespec>().tv_nsec);

        auto const secs = std::chrono::duration_cast<std::chrono::duration<sec_t>>(time.time_since_epoch());
        auto const nsecs = std::chrono::duration_cast<std::chrono::duration<nsec_t, std::nano>>(time.time_since_epoch() - secs);

        auto spec = std::make_unique<__kernel_timespec>(__kernel_timespec { secs.count(), nsecs.count() });

        return tcx::impl::wrap_op<tcx::impl::ioring_timeout_operation<true>>::call(executor, service, std::forward<F>(f), std::move(spec));
    } else {
        auto const duration = time - Clock::now();
        return tcx::async_sleep_for(executor, duration, std::forward<F>(f));
    }
}

}

#endif