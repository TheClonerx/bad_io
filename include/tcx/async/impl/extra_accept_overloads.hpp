#ifndef TCX_ASYNC_IMPL_EXTRA_ACCEPT_OVERLOADS_HPP
#define TCX_ASYNC_IMPL_EXTRA_ACCEPT_OVERLOADS_HPP

#include <tcx/async/concepts.hpp>
#include <tcx/native/handle.hpp>

#include <sys/socket.h>

namespace tcx {

template <typename E, typename F, typename S>
requires tcx::completion_handler<F, tcx::native::handle_type>
auto async_accept(E &executor, S &service, tcx::native::handle_type fd, sockaddr *addr, std::size_t *addr_len, F &&f)
{
    return tcx::async_accept(executor, service, fd, addr, addr_len, 0, std::forward<F>(f));
}

template <typename E, typename F, typename S>
requires tcx::completion_handler<F, tcx::native::handle_type>
auto async_accept(E &executor, S &service, tcx::native::handle_type fd, int flags, F &&f)
{
    return tcx::async_accept(executor, service, fd, nullptr, nullptr, flags, std::forward<F>(f));
}

template <typename E, typename F, typename S>
requires tcx::completion_handler<F, tcx::native::handle_type>
auto async_accept(E &executor, S &service, tcx::native::handle_type fd, F &&f)
{
    return tcx::async_accept(executor, service, fd, nullptr, nullptr, 0, std::forward<F>(f));
}

} // namespace tcx

#endif
