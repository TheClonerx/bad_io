#ifndef TCX_ASYNC_READ_HPP

#include <coroutine>
#include <system_error>
#include <type_traits>

#ifdef __linux__
#include <tcx/services/ioring_service.hpp>
#else

#endif

#include <tcx/async/use_awaitable.hpp>
#include <tcx/native/handle.hpp>

namespace tcx {

template <typename E, typename F>
void async_read(E &executor, tcx::native_handle_type fd, void *buf, std::size_t len, F &&f) requires(std::is_invocable_v<F, std::error_code, std::size_t>)
{
#ifdef __linux__
    executor.template use_service<tcx::ioring_service>().async_read(0, fd, buf, len, [f = std::forward<F>(f)](std::int32_t result) {
        if (result < 0)
            f(std::error_code { -result, std::system_category() }, static_cast<std::size_t>(0));
        else
            f(std::error_code {}, static_cast<std::size_t>(result));
    });
#else

#endif
}

template <typename E>
auto async_read(E &executor, int fd, void *buf, std::size_t len, use_awaitable_t) noexcept
{
    struct awaiter {
        constexpr bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle)
        {
            this->handle = handle;
            tcx::async_read(executor, fd, buf, len, [this](std::error_code e, std::size_t result) {
                this->e = e;
                this->result = result;
                this->handle.resume();
            });
        }

        std::size_t await_resume() const
        {
            if (e)
                throw std::system_error(e);
            return result;
        }

        E &executor;
        int fd;
        void *buf;
        std::size_t len;

        std::coroutine_handle<> handle;
        std::error_code e;
        std::size_t result;
    };
    return awaiter { executor, fd, buf, len };
}

}

#endif