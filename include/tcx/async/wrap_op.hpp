#ifndef TCX_ASYN_IMPL_WRAP_OPERATION_HPP
#define TCX_ASYN_IMPL_WRAP_OPERATION_HPP

#include <tcx/async/concepts.hpp>
#include <type_traits>
#include <utility>

namespace tcx::impl {

template <typename Op>
struct wrap_op {
    template <typename E, typename S, typename F, typename... Args>
    static auto call(E &executor, S &service, F &&f, Args &&...args)
    {
        if constexpr (tcx::impl::has_async_transform<F, typename Op::result_type>) {
            return call(executor, service, f.template async_transform<typename Op::result_type>(), std::forward<Args>(args)...);
        } else if constexpr (tcx::impl::has_async_result<F>) {
            auto result = f.async_result();
            execute(executor, service, std::forward<F>(f), std::forward<Args>(args)...);
            return result;
        } else {
            execute(executor, service, std::forward<F>(f), std::forward<Args>(args)...);
        }
    }

private:
    template <typename E, typename S, typename F, typename... Args>
    static void execute(E &executor, S &service, F &&f, Args &&...args)
    {
        if constexpr (std::is_void_v<typename Op::result_type>) {
            if constexpr (std::is_invocable_v<F, std::variant<std::error_code, std::monostate>>) {
                Op::call(executor, service, std::forward<Args>(args)..., [f = std::forward<F>(f)](std::error_code e) {
                    if (e)
                        f(std::variant<std::error_code, std::monostate>(std::in_place_index<0>, e));
                    else
                        f(std::variant<std::error_code, std::monostate>(std::in_place_index<1>));
                });
            } else {
                Op::call(executor, service, std::forward<Args>(args)..., std::forward<F>(f));
            }
        } else {
            if constexpr (std::is_invocable_v<F, std::variant<std::error_code, typename Op::result_type>>) {
                Op::call(executor, service, std::forward<Args>(args)..., [f = std::forward<F>(f)](std::error_code e, typename Op::result_type result) {
                    if (e)
                        f(std::variant<std::error_code, typename Op::result_type>(std::in_place_index<0>, e));
                    else
                        f(std::variant<std::error_code, typename Op::result_type>(std::in_place_index<1>, result));
                });
            } else {
                Op::call(executor, service, std::forward<Args>(args)..., std::forward<F>(f));
            }
        }
    }
};

}

#endif