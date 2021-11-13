#ifndef TCX_ASYNC_impl_CONCEPTS_HPP
#define TCX_ASYNC_impl_CONCEPTS_HPP

#include <concepts>
#include <system_error>
#include <type_traits>
#include <variant>

namespace tcx {

namespace impl {

    template <typename T>
    concept non_void = !std::is_void_v<T>;

    // clang-format kind of dies here

    template <typename F, typename T>
    concept has_async_transform = requires(F &&f)
    {
        {
            f.template async_transform<T>()
            } -> non_void;
    };

    template <typename F>
    concept has_async_result = requires(F &&f)
    {
        {
            f.async_result()
        };
    };

    template <typename F, typename T>
    constexpr bool is_completion_handler()
    {
        if constexpr (has_async_transform<F, T>)
            return is_completion_handler<decltype(std::declval<F>().template async_transform<T>()), T>();
        else if constexpr (std::is_void_v<T>)
            return std::invocable<F, std::error_code> || std::invocable<F, std::variant<std::error_code, std::monostate>>;
        else
            return std::invocable<F, std::error_code, T> || std::invocable<F, std::variant<std::error_code, T>>;
    }
}

/**
 * @brief Specifies that a type can be used as a completion handler of an asynchronous operation.
 *
 * If the expression `o.template async_transform<R>()` (where `o` is an instance of `T`)
 * is valid and it's type is not *cv-qualified* `void` then that type is used as `T` instead (recursively).
 *
 * If `R` is *cv-qualified* `void`, then `T` must be invocable with a single `std::error_code` argument,
 * or a `std::variant<std::error_code, std::monostate>`.
 *
 * If `R` is not *cv-qualified* `void`, then `T` must be invocable with the arguments `std::error_code` and `R`,
 * or a `std::variant<std::error_code, R>`.
 *
 * @tparam R Result type of the asynchronous operation.
 */
#ifdef DOXYGEN_INVOKED
template <typename T, typename R>
concept completion_handler = implementation defined;
#else
template <typename T, typename R>
concept completion_handler = impl::is_completion_handler<T, R>();
#endif
}

#endif