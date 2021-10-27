#ifndef TCX_ASYNC_impl_CONCEPTS_HPP
#define TCX_ASYNC_impl_CONCEPTS_HPP

#include <concepts>
#include <system_error>
#include <type_traits>

namespace tcx {

namespace impl {

    // clang-format kind dies here

    template <typename F, typename T>
        struct has_async_transform_t : std::bool_constant < requires(F &&f) {
        {
            f.template async_transform<T>()
            } -> std::invocable<std::error_code, T>;
    } > {};

    template <typename F>
        struct has_async_transform_t<F, void> : std::bool_constant < requires(F &&f) {

        {
            f.template async_transform<void>()
            }
            -> std::invocable<std::error_code>;
    } > {};

    template <typename F>
    concept has_async_result = requires(F &&f)
    {
        { f.async_result() };
    };

    template <typename F, typename T>
    struct completion_handler_t : std::bool_constant<impl::has_async_transform_t<F, T>::value || std::invocable<F, std::error_code, T>> {
    };

    template <typename F>
    struct completion_handler_t<F, void> : std::bool_constant<impl::has_async_transform_t<F, void>::value || std::invocable<F, std::error_code>> {
    };

}

template <typename F, typename T>
concept completion_handler = impl::completion_handler_t<F, T>::value;

}

#endif