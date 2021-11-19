#ifndef TCX_ASYNC_IORING_WRITE_HPP
#define TCX_ASYNC_IORING_WRITE_HPP

#include <cstdio>
#include <utility>

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

namespace impl {
    struct ioring_write_operation {
        using result_type = std::size_t;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t len, off_t offset, F &&f)
        {
            service.async_write(fd, buf, len, offset, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                executor.post([f = std::move(f), result]() mutable {
                    if (result < 0)
                        f(std::error_code { -result, std::system_category() }, static_cast<std::size_t>(0));
                    else
                        f(std::error_code {}, static_cast<std::size_t>(result));
                });
            });
        }
    };
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_write_operation::result_type>
auto async_write(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void const *buf, std::size_t len, off_t offset, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_write_operation>::call(executor, service, std::forward<F>(f), fd, buf, len, offset);
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_write_operation::result_type>
auto async_write(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void const *buf, std::size_t len, F &&f)
{
    return async_read(executor, service, fd, buf, len, -1, std::forward<F>(f));
}

}

#endif