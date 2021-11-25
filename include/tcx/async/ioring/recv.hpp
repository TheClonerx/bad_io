#ifndef TCX_ASYNC_IORING_RECV_HPP
#define TCX_ASYNC_IORING_RECV_HPP

#include <cstddef>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

#include <span>
#include <utility>

namespace tcx {
namespace impl {
    struct ioring_recv_operation {
        using result_type = std::size_t;

        template <typename E, typename S, typename F>
        static void call(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t buf_len, int flags, F &&f)
        {
            service.async_recv(fd, buf, buf_len, flags, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
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

template <typename E, typename F>
auto async_recv(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t buf_len, int flags, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_recv_operation>::call(executor, service, std::forward<F>(f), fd, buf, buf_len, flags);
}

template <typename E, typename F>
auto async_recv(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t buf_len, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_recv_operation>::call(executor, service, std::forward<F>(f), fd, buf, buf_len);
}

template <typename E, typename F, std::size_t Extent>
auto async_recv(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, std::span<std::byte, Extent> bytes, int flags, F &&f)
{
    return tcx::async_recv(executor, service, fd, bytes.data(), bytes.size(), flags, std::forward<F>(f));
}

template <typename E, typename F, std::size_t Extent>
auto async_recv(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, std::span<std::byte, Extent> bytes, F &&f)
{
    return tcx::async_recv(executor, service, fd, bytes.data(), bytes.size(), 0, std::forward<F>(f));
}

}

#endif