#ifndef TCX_ASYNC_IORING_POLL_HPP
#define TCX_ASYNC_IORING_POLL_HPP

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/uring_service.hpp>

namespace tcx {
namespace impl {

    struct ioring_poll_operation {
        using result_type = std::uint32_t;

        template <typename E, typename F>
        static auto call(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, std::uint32_t events, F &&f)
        {
            return service.async_poll_add(fd, events, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                return executor.post([f = std::move(f), result]() mutable {
                    if (result < 0)
                        return f(std::error_code { -result, std::system_category() }, 0);
                    else
                        return f(std::error_code {}, result);
                });
            });
        }
    };

} // namespace impl

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_poll_operation::result_type>
auto async_poll(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, std::uint32_t events, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_poll_operation>::call(executor, service, std::forward<F>(f), fd, events);
}

} // namespace tcx

#endif
