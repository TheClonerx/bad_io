#ifndef TCX_ASYN_IMPL_WRAP_OPERATION_HPP
#define TCX_ASYN_IMPL_WRAP_OPERATION_HPP

#include <tcx/async/concepts.hpp>
#include <type_traits>
#include <utility>

namespace tcx::impl {

template <typename Op>
struct wrap_op {
    template <typename E, typename S, typename F, typename... Args>
    static decltype(auto) call(E &executor, S &service, F &&f, Args &&...args)
    {
        if constexpr (tcx::impl::has_async_transform<F, typename Op::result_type>) {
            return call(executor, service, f.template async_transform<typename Op::result_type>(), std::forward<Args>(args)...);
        } else if constexpr (tcx::impl::has_async_result<F>) {
            auto result = f.async_result();
            Op::call(executor, service, std::forward<Args>(args)..., std::forward<F>(f));
            return result;
        } else {
            Op::call(executor, service, std::forward<Args>(args)..., std::forward<F>(f));
        }
    }
};

} // namespace tcx::impl

#endif
