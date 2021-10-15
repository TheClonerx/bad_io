#ifndef TCX_ASYNC_IORING_CLOSE_HPP

#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

template <typename E, typename F>
void async_close(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, F &&f) requires(std::is_invocable_v<F, std::error_code>)
{
    service.async_close(fd, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
        if (result < 0)
            executor.post([f = std::move(f), result]() mutable {
                f(std::error_code { -result, std::system_category() });
            });
        else
            executor.post([f = std::move(f), result]() mutable {
                f(std::error_code {});
            });
    });
}

}

#endif
