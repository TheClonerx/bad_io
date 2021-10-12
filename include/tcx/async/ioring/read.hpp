
#ifndef TCX_ASYNC_IORING_READ_HPP

#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

template <typename E, typename F>
void async_read(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t len, F &&f) requires(std::is_invocable_v<F, std::error_code, std::size_t>)
{
    (void)executor;

    service.async_read(fd, buf, len, -1, [f = std::forward<F>(f)](std::int32_t result) mutable {
        if (result < 0)
            f(std::error_code { -result, std::system_category() }, static_cast<std::size_t>(0));
        else
            f(std::error_code {}, static_cast<std::size_t>(result));
    });
}
}

#endif