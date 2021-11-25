#ifndef TCX_ASYNC_IORING_CONNECT_HPP
#define TCX_ASYNC_IORING_CONNECT_HPP

#include <sys/socket.h>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

#include <utility>

namespace tcx {
namespace impl {

    struct ioring_connect_operation {
        using result_type = void;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, sockaddr const *addr, std::size_t const *addr_len, int flags, F &&f)
        {
            if (addr_len == nullptr) [[unlikely]] {
                service.async_connect(fd, addr, nullptr, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    executor.post([f = std::move(f), result]() mutable {
                        if (result < 0)
                            f(std::error_code { -result, std::system_category() });
                        else
                            f(std::error_code {});
                    });
                });
            } else {
                auto sock_len = std::make_unique<socklen_t>(addr_len);
                auto const p = sock_len.get();
                service.async_connect(fd, addr, p, [sock_len = std::move(sock_len), &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    sock_len.reset();
                    executor.post([f = std::move(f), result]() mutable {
                        if (result < 0)
                            f(std::error_code { -result, std::system_category() }, static_cast<std::size_t>(0));
                        else
                            f(std::error_code {});
                    });
                });
            }
        }
    };

}

template <typename E, typename F>
auto async_connect(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, sockaddr const *addr, std::size_t const *addr_len, int flags, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_connect_operation>::call(executor, service, std::forward<F>(f), fd, addr, addr_len, flags);
}

template <typename E, typename F>
auto async_connect(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, sockaddr const *addr, std::size_t const *addr_len, F &&f)
{
    return tcx::async_connect(executor, service, fd, addr, addr_len, 0, std::forward<F>(f));
}

template <typename E, typename F>
auto async_connect(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, int flags, F &&f)
{
    return tcx::async_connect(executor, service, fd, nullptr, nullptr, flags, std::forward<F>(f));
}

template <typename E, typename F>
auto async_connect(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, F &&f)
{
    return tcx::async_connect(executor, service, fd, nullptr, nullptr, 0, std::forward<F>(f));
}

}

#endif