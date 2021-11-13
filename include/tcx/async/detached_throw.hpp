#ifndef TCX_ASYNC_DETACHED_THROW_HPP
#define TCX_ASYNC_DETACHED_THROW_HPP

#include <system_error>

namespace tcx {

/**
 * @brief An invocable type that accepts an error, and ignores the rest of the arguments.

 * In case of an error, an `std::system_error` is thrown with the value of the error.
 * @see tcx::detached_throw
 * @see tcx::detached_t
 */
struct detached_throw_t {
    explicit constexpr detached_throw_t() noexcept = default;

    template <typename... Args>
    void operator()(std::error_code ec, Args &&...) const
    {
        if (ec)
            throw std::system_error(ec);
    };
};

/**
 * @brief Specifies that an asynchronous operation is detached.
 * In case of an error, an `std::system_error` is thrown with the value of the error.
 * @see tcx::detached_throw_t
 */
inline constexpr auto detached_throw = detached_throw_t {};

}

#endif