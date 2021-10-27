#ifndef TCX_ASYNC_IORING_CLOSE_HPP

#include <tcx/async/concepts.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

namespace impl {

    template <typename E, typename F>
    void async_close(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, F &&f)
    {
        service.async_close(fd, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
            executor.post([f = std::move(f), result]() mutable {
                if (result < 0)
                    f(std::error_code { -result, std::system_category() });
                else
                    f(std::error_code {});
            });
        });
    }

}

template <typename E, typename F>
requires tcx::completion_handler<F, void>
auto async_close(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, F &&f)
{
    if constexpr (tcx::impl::has_async_transform_t<F, void>::value) {
        return async_close(executor, service, fd, f.template async_transform<void>());
    } else if constexpr (tcx::impl::has_async_result<F>) {
        auto result = f.async_result();
        impl::async_close(executor, service, fd, std::forward<F>(f));
        return result;
    } else {
        impl::async_close(executor, service, fd, std::forward<F>(f));
    }
}

}

#endif
