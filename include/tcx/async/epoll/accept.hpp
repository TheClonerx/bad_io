#ifndef TCX_ASYNC_EPOLL_ACCEPT_HPP
#define TCX_ASYNC_EPOLL_ACCEPT_HPP

#include <sys/epoll.h>
#include <system_error>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/epoll_service.hpp>

#include <memory>
#include <unistd.h>
#include <utility>

#include <sys/socket.h> // struct ::sockaddr, using ::socklen_t

namespace tcx {
namespace impl {
    struct epoll_accept_operation {
        using result_type = tcx::native_handle_type;

        template <typename E, typename F>
        static void call(E &executor, tcx::epoll_service &service, tcx::native_handle_type fd, sockaddr *addr, std::size_t *addr_len, int flags, F &&f)
        {

            service.async_poll_add(fd, EPOLLIN, [&executor, f = std::forward<F>(f), addr, addr_len, flags, fd](std::int32_t result) mutable {
                executor.post([f = std::move(f), result, addr, addr_len, flags, fd]() mutable {
                    if (result < 0)
                        f(std::error_code { -result, std::system_category() }, tcx::invalid_handle);
                    else {
                        if (addr_len == nullptr) {
                            int new_fd = ::accept4(fd, addr, nullptr, flags);
                            if (new_fd >= 0)

                                f(std::error_code {}, new_fd);
                        } else {
                            socklen_t real_len = *addr_len;
                            int new_fd = ::accept4(fd, addr, &real_len, flags);
                            if (new_fd >= 0) {
                                *addr_len = real_len;
                                f(std::error_code {}, new_fd);
                            }
                        }
                        f(std::error_code { errno, std::system_category() }, tcx::invalid_handle);
                    };
                });
            });
        }
    };
}

template <typename E, typename F>
    requires tcx::completion_handler<F, tcx::impl::epoll_accept_operation::result_type>
decltype(auto) async_accept(E &executor, tcx::epoll_service &service, tcx::native_handle_type fd, sockaddr *addr, std::size_t *addr_len, int flags, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::epoll_accept_operation>::call(executor, service, std::forward<F>(f), fd, addr, addr_len, flags);
}

}

#endif