#ifndef TCX_ASYNC_IORING_ACCEPT_HPP
#define TCX_ASYNC_IORING_ACCEPT_HPP

#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/uring_service.hpp>

#include <memory>
#include <utility>

#include <sys/socket.h> // struct ::sockaddr, using ::socklen_t

namespace tcx {
namespace impl {
    struct ioring_accept_operation {
        using result_type = tcx::native::handle_type;

        template <typename E, typename F>
        static auto call(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, sockaddr *addr, std::size_t *addr_len, int flags, F &&f)
        {
            if (addr_len == nullptr) {
                return service.async_accept(fd, addr, nullptr, flags, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    return executor.post([f = std::move(f), result]() mutable {
                        if (result < 0)
                            return f(std::error_code { -result, std::system_category() }, tcx::native::invalid_handle);
                        else
                            return f(std::error_code {}, result);
                    });
                });
            } else {
                auto sock_len = std::make_unique<socklen_t>();
                auto const p = sock_len.get();
                return service.async_accept(fd, addr, p, flags, [sock_len = std::move(sock_len), addr_len, &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                    return executor.post([sock_len = std::move(sock_len), addr_len, f = std::move(f), result]() mutable {
                        if (result < 0)
                            return f(std::error_code { -result, std::system_category() }, tcx::native::invalid_handle);
                        else {
                            *addr_len = *sock_len;
                            sock_len.reset();
                            return f(std::error_code {}, result);
                        }
                    });
                });
            }
        }
    };

} // namespace impl

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_accept_operation::result_type>
auto async_accept(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, sockaddr *addr, std::size_t *addr_len, int flags, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_accept_operation>::call(executor, service, std::forward<F>(f), fd, addr, addr_len, flags);
}

} // namespace tcx

#include <tcx/async/impl/extra_accept_overloads.hpp>

#endif
