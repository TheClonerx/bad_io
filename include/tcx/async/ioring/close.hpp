#ifndef TCX_ASYNC_IORING_CLOSE_HPP
#define TCX_ASYNC_IORING_CLOSE_HPP

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/uring_service.hpp>

namespace tcx {
namespace impl {

    struct ioring_close_operation {
        using result_type = void;

        template <typename E, typename F>
        static auto call(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, F &&f)
        {
            return service.async_close(fd, [&executor, f = std::forward<F>(f)](tcx::uring_context auto &, io_uring_cqe const *result) mutable {
                return executor.post([f = std::move(f), result = result->res]() mutable {
                    if (result < 0)
                        return f(std::error_code { -result, std::system_category() });
                    else
                        return f(std::error_code {});
                });
            });
        }
    };

} // namespace impl

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_close_operation::result_type>
auto async_close(E &executor, tcx::uring_context auto &service, tcx::native::handle_type fd, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_close_operation>::call(executor, service, std::forward<F>(f), fd);
}

} // namespace tcx

#endif
