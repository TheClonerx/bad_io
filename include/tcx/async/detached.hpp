#ifndef TCX_ASYNC_DETACHED_HPP
#define TCX_ASYNC_DETACHED_HPP

namespace tcx {

/**
 * @brief An invocable type that accepts and ignores the arguments.
 * @see tcx::detached
 * @see tcx::detached_throw_t
 */
struct detached_t {
    explicit constexpr detached_t() noexcept = default;

    template <typename... Args>
    constexpr void operator()(Args &&...) const noexcept {};
};

/**
 * @brief Specifies that an asynchronous operation is detached. Both the error and result are discarted.
 * @see tcx::detached_t
 */
inline constexpr auto detached = detached_t {};

}

#endif