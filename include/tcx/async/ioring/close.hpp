#ifndef TCX_ASYNC_IORING_CLOSE_HPP

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

namespace impl {

    struct ioring_close_operation {
        using result_type = void;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, F &&f)
        {
            service.async_close(fd, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
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

template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_close_operation::result_type>
auto async_close(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_close_operation>::call(executor, service, std::forward<F>(f), fd);
}

}

#endif
