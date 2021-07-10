#ifndef TCX_ASYNC_OPEN_HPP
#define TCX_ASYNC_OPEN_HPP

#include <coroutine>
#include <string_view>
#include <system_error>
#include <type_traits>

#ifdef __linux__

#include <tcx/services/ioring_service.hpp>
#else

#endif

#include <tcx/async/use_awaitable.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/native/path.hpp>

namespace tcx {

enum open_mode {
    read,
    write,
    truncate,
    append,
    no_create
};

template <typename E, typename F>
void async_open(E& executor, std::basic_string_view<tcx::native_path_char_type> path, std::underlying_type_t<tcx::open_mode> mode, F&& f) requires(std::is_invocable_v<F, std::error_code, tcx::native_handle_type>)
{
#ifdef __linux__
    bool const has_read = mode & open_mode::read;
    bool const has_write = mode & open_mode::write;
    bool const has_append = mode & open_mode::append;
    bool const has_truncate = mode & open_mode::truncate;
    bool const has_no_create = mode & open_mode::no_create;
    int m = O_TRUNC * has_truncate | O_APPEND * has_append | O_CREAT * has_write * !has_no_create;
    if (has_read && has_write)
        m |= O_RDWR;
    else
        m |= O_RDONLY * has_read | O_WRONLY * has_write;
    executor.template use_service<tcx::ioring_service>().async_open(0, path.data(), m, 0, [f = std::forward<F>(f)](std::int32_t result) {
        if (result < 0)
            f(std::error_code { -result, std::system_category() }, tcx::invalid_handle);
        else
            f(std::error_code {}, result);
    });
#else
#endif
}

template <typename E>
auto async_open(E& executor, std::basic_string_view<tcx::native_path_char_type> path, std::underlying_type_t<tcx::open_mode> mode, tcx::use_awaitable_t)
{
    struct awaiter {
        constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h)
        {
            handle = h;
            tcx::async_open(executor, path, mode, [this](std::error_code e, tcx::native_handle_type fd) {
                this->e = e;
                this->fd = fd;
                this->handle.resume();
            });
        }

        tcx::native_handle_type await_resume() const
        {
            if (e)
                throw std::system_error(e);
            return fd;
        }
        E& executor;
        std::basic_string_view<tcx::native_path_char_type> path;
        std::underlying_type_t<open_mode> mode;

        std::coroutine_handle<> handle;
        std::error_code e;
        tcx::native_handle_type fd;
    };
    return awaiter { executor, path, mode };
}

}

#endif