#ifndef TCX_ASYNC_IORING_SEND_HPP
#define TCX_ASYNC_IORING_SEND_HPP

#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

#include <span>
#include <utility>

namespace tcx {
namespace impl {
    struct ioring_send_operation {
        using result_type = std::size_t;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, tcx::native::handle_type fd, void const *buf, std::size_t buf_len, int flags, F &&f)
        {
            service.async_send(fd, buf, buf_len, flags, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                executor.post([f = std::move(f), result]() mutable {
                    if (result < 0)
                        f(std::error_code { -result, std::system_category() }, static_cast<std::size_t>(0));
                    else
                        f(std::error_code {}, static_cast<std::size_t>(result));
                });
            });
        }
    };
} // namespace impl

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_send_operation::result_type>
auto async_send(E &executor, tcx::ioring_service &service, tcx::native::handle_type fd, void const *buf, std::size_t buf_len, int flags, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_send_operation>::call(executor, service, std::forward<F>(f), fd, buf, buf_len, flags);
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_send_operation::result_type>
auto async_send(E &executor, tcx::ioring_service &service, tcx::native::handle_type fd, void const *buf, std::size_t buf_len, F &&f)
{
    return tcx::async_send(executor, service, fd, buf, buf_len, 0, std::forward<F>(f));
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F, std::size_t Extent>
requires tcx::completion_handler<F, tcx::impl::ioring_send_operation::result_type>
auto async_send(E &executor, tcx::ioring_service &service, tcx::native::handle_type fd, std::span<std::byte const, Extent> bytes, int flags, F &&f)
{
    return tcx::async_send(executor, service, fd, bytes.data(), bytes.size(), flags, std::forward<F>(f));
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F, std::size_t Extent>
requires tcx::completion_handler<F, tcx::impl::ioring_send_operation::result_type>
auto async_send(E &executor, tcx::ioring_service &service, tcx::native::handle_type fd, std::span<std::byte const, Extent> bytes, F &&f)
{
    return tcx::async_send(executor, service, fd, bytes.data(), bytes.size(), 0, std::forward<F>(f));
}

} // namespace tcx

#endif
