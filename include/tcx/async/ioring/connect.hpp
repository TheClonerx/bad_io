#ifndef TCX_ASYNC_IORING_CONNECT_HPP
#define TCX_ASYNC_IORING_CONNECT_HPP

#include <sys/socket.h>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/uring_service.hpp>

#include <utility>

namespace tcx {
namespace impl {

    struct ioring_connect_operation {
        using result_type = void;

        template <typename E, typename F>
        static auto call(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, sockaddr const *addr, std::size_t const *addr_len, F &&f)
        {
            if (addr_len == nullptr) [[unlikely]] {
                return service.async_connect(fd, addr, nullptr, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    return executor.post([f = std::move(f), result]() mutable {
                        if (result < 0)
                            return f(std::error_code { -result, std::system_category() });
                        else
                            return f(std::error_code {});
                    });
                });
            } else {
                auto sock_len = std::make_unique<socklen_t>(addr_len);
                auto const p = sock_len.get();
                return service.async_connect(fd, addr, p, [sock_len = std::move(sock_len), &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    sock_len.reset();
                    return executor.post([f = std::move(f), result]() mutable {
                        if (result < 0)
                            return f(std::error_code { -result, std::system_category() }, static_cast<std::size_t>(0));
                        else
                            return f(std::error_code {});
                    });
                });
            }
        }
    };

} // namespace impl

template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_connect_operation::result_type>
auto async_connect(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, sockaddr const *addr, std::size_t const *addr_len, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_connect_operation>::call(executor, service, std::forward<F>(f), fd, addr, addr_len);
}

template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_connect_operation::result_type>
auto async_connect(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, F &&f)
{
    return tcx::async_connect(executor, service, fd, nullptr, nullptr, 0, std::forward<F>(f));
}

} // namespace tcx

#endif
