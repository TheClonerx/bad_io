#include <sys/stat.h>
#ifndef TCX_ASYNC_IORING_READ_HPP

#include <cstdio>
#include <utility>

#include <tcx/async/concepts.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

namespace impl {
    template <typename E, typename F>
    void async_read(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t len, off_t offset, F &&f)
    {
        service.async_read(fd, buf, len, offset, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
            executor.post([f = std::move(f), result]() mutable {
                if (result < 0)
                    f(std::error_code { -result, std::system_category() }, static_cast<std::size_t>(0));
                else
                    f(std::error_code {}, static_cast<std::size_t>(result));
            });
        });
    }
}

template <typename E, typename F>
requires tcx::completion_handler<F, std::size_t>
auto async_read(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t len, off_t offset, F &&f)
{
    if constexpr (tcx::impl::has_async_transform_t<F, std::size_t>::value) {
        return async_read(executor, service, fd, buf, len, offset, f.template async_transform<std::size_t>());
    } else if constexpr (tcx::impl::has_async_result<F>) {
        auto result = f.async_result();
        impl::async_read(executor, service, fd, buf, len, offset, std::forward<F>(f));
        return result;
    } else {
        impl::async_read(executor, service, fd, buf, len, offset, std::forward<F>(f));
    }
}

template <typename E, typename F>
requires tcx::completion_handler<F, std::size_t>
auto async_read(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, void *buf, std::size_t len, F &&f)
{
    return async_read(executor, service, fd, buf, len, -1, std::forward<F>(f));
}

}

#endif