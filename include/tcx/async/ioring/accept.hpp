#ifndef TCX_ASYNC_IORING_ACCEPT_HPP
#define TCX_ASYNC_IORING_ACCEPT_HPP

#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

#include <memory>
#include <utility>

#include <sys/socket.h> // struct ::sockaddr, using ::socklen_t

namespace tcx {
namespace impl {
    struct ioring_accept_operation {
        using result_type = tcx::native_handle_type;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, sockaddr *addr, std::size_t *addr_len, int flags, F &&f)
        {
            if (addr_len == nullptr) {
                service.async_accept(fd, addr, nullptr, flags, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    executor.post([f = std::move(f), result]() mutable {
                        if (result < 0)
                            f(std::error_code { -result, std::system_category() }, tcx::invalid_handle);
                        else
                            f(std::error_code {}, result);
                    });
                });
            } else {
                auto sock_len = std::make_unique<socklen_t>();
                auto const p = sock_len.get();
                service.async_accept(fd, addr, p, flags, [sock_len = std::move(sock_len), addr_len, &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    executor.post([sock_len = std::move(sock_len), addr_len, f = std::move(f), result]() mutable {
                        if (result < 0)
                            f(std::error_code { -result, std::system_category() }, tcx::invalid_handle);
                        else {
                            *addr_len = *sock_len;
                            sock_len.reset();
                            f(std::error_code {}, result);
                        }
                    });
                });
            }
        }
    };

}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_accept_operation::result_type>
auto async_accept(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, sockaddr *addr, std::size_t *addr_len, int flags, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_accept_operation>::call(executor, service, std::forward<F>(f), fd, addr, addr_len, flags);
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_accept_operation::result_type>
auto async_accept(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, sockaddr *addr, std::size_t *addr_len, F &&f)
{
    return tcx::async_accept(executor, service, fd, addr, addr_len, 0, std::forward<F>(f));
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_accept_operation::result_type>
auto async_accept(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, int flags, F &&f)
{
    return tcx::async_accept(executor, service, fd, nullptr, nullptr, flags, std::forward<F>(f));
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_accept_operation::result_type>
auto async_accept(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, F &&f)
{
    return tcx::async_accept(executor, service, fd, nullptr, nullptr, 0, std::forward<F>(f));
}

}

#endif