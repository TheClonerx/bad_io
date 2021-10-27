#ifndef TCX_ASYNC_IORING_POLL_HPP

#include <tcx/async/concepts.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

namespace impl {

    template <typename E, typename F>
    void async_poll(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, std::uint32_t events, F &&f)
    {
        service.async_poll_add(fd, events, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
            executor.post([f = std::move(f), result]() mutable {
                if (result < 0)
                    f(std::error_code { -result, std::system_category() }, 0);
                else
                    f(std::error_code {}, result);
            });
        });
    }
}

template <typename E, typename F>
requires tcx::completion_handler<F, std::uint32_t>
auto async_poll(E &executor, tcx::ioring_service &service, tcx::native_handle_type fd, std::uint32_t events, F &&f)
{
    if constexpr (tcx::impl::has_async_transform_t<F, std::uint32_t>::value) {
        return async_poll(executor, service, fd, events, f.template async_transform<std::uint32_t>());
    } else if constexpr (tcx::impl::has_async_result<F>) {
        auto result = f.async_result();
        impl::async_poll(executor, service, fd, events, std::forward<F>(f));
        return result;
    } else {
        impl::async_poll(executor, service, fd, events, std::forward<F>(f));
    }
}

}

#endif
