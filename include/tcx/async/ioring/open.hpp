#ifndef TCX_ASYNC_IORING_OPEN_HPP
#define TCX_ASYNC_IORING_OPEN_HPP

#include <stdexcept>
#include <system_error>
#include <type_traits>

#include <fcntl.h>

#include <tcx/native/handle.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {

template <typename E, typename F>
void async_open(E &executor, tcx::ioring_service &service, char const *path, int flags, mode_t mode, F &&f) requires(std::is_invocable_v<F, std::error_code, tcx::native_handle_type>)
{
    service.async_open(path, flags, mode, [&executor, f = std::forward<F>(f)](std::int32_t result) mutable {
        if (result < 0)
            executor.post([f = std::move(f), result]() mutable {
                f(std::error_code { -result, std::system_category() }, tcx::invalid_handle);
            });
        else
            executor.post([f = std::move(f), result]() mutable {
                f(std::error_code {}, result);
            });
    });
}

template <typename E, typename F>
auto async_open(E &executor, tcx::ioring_service &service, char const *path, char const *mode, F &&f) requires(std::is_invocable_v<F, std::error_code, tcx::native_handle_type>)
{
    bool has_plus = false, has_read = false, has_write = false, has_append = false, has_cloexec = false, has_exclusive = false;
    for (auto *it = mode; *it && *it != ','; ++it) {
        switch (*it) {
        case '+':
            has_plus = true;
            break;
        case 'r':
            has_read = true;
            break;
        case 'w':
            has_write = true;
            break;
        case 'a':
            has_append = true;
            break;
        case 'e':
            has_cloexec = true;
            break;
        case 'x':
            has_exclusive = true;
            break;
        default:
            break;
        }
    }

    if (has_read + has_write + has_append != 1)
        throw std::invalid_argument("Invalid mode was provided");

    int flags = 0;
    if (has_plus)
        flags |= (O_RDWR | 0) * has_read | (O_RDWR | O_CREAT | O_TRUNC) * has_write | (O_RDWR | O_CREAT | O_APPEND) * has_append;
    else
        flags |= (O_RDONLY | 0) * has_read | (O_WRONLY | O_CREAT | O_TRUNC) * has_write | (O_WRONLY | O_CREAT | O_APPEND) * has_append;
    flags |= O_CLOEXEC * has_cloexec | O_EXCL * has_exclusive;

    return async_open(executor, service, path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, std::forward<F>(f));
}

}

#endif