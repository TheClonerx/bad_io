#ifndef TCX_ASYNC_IORING_OPEN_HPP
#define TCX_ASYNC_IORING_OPEN_HPP

#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#include <fcntl.h>

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/services/uring_service.hpp>

namespace tcx {
namespace impl {
    struct ioring_open_operation {
        using result_type = tcx::native::handle_type;

        template <typename E, typename F>
        static auto call(E &executor, tcx::uring_context auto &service, char const *path, int flags, mode_t mode, F &&f)
        {
            using variant_type = std::variant<std::error_code, result_type>;

            return service.async_open(path, flags, mode, [&executor, f = std::forward<F>(f)](tcx::uring_context auto &, io_uring_cqe const *result) mutable {
                return executor.post([f = std::move(f), result = result->res]() mutable {
                    if (result < 0)
                        return f(variant_type(std::in_place_index<0>, -result, std::system_category()));
                    else
                        return f(variant_type(std::in_place_index<1>, result));
                });
            });
        }
    };
} // namespace impl

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_open_operation::result_type>
auto async_open(E &executor, tcx::uring_context auto &service, char const *path, int flags, mode_t mode, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_open_operation>::call(executor, service, std::forward<F>(f), path, flags, mode);
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_open_operation::result_type>
auto async_open(E &executor, tcx::uring_context auto &service, char const *path, char const *mode, F &&f)
{
    bool has_plus = false, has_read = false, has_write = false, has_append = false, has_cloexec = false, has_exclusive = false;
    for (auto const *it = mode; *it && *it != ','; ++it) {
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
            /* unkown modes are ignored */
            break;
        }
    }

    if (has_read + has_write + has_append != 1)
        throw std::invalid_argument("invalid mode was provided");

    int flags = O_CLOEXEC * has_cloexec | O_EXCL * has_exclusive;
    if (has_plus)
        flags |= (O_RDWR | 0) * has_read | (O_RDWR | O_CREAT | O_TRUNC) * has_write | (O_RDWR | O_CREAT | O_APPEND) * has_append;
    else
        flags |= (O_RDONLY | 0) * has_read | (O_WRONLY | O_CREAT | O_TRUNC) * has_write | (O_WRONLY | O_CREAT | O_APPEND) * has_append;

    return tcx::async_open(executor, service, path, flags, DEFFILEMODE, std::forward<F>(f));
}

} // namespace tcx

#endif
