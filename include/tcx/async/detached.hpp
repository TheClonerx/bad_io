#ifndef TCX_ASYNC_DETACHED_HPP
#define TCX_ASYNC_DETACHED_HPP

namespace tcx {

struct detached_t {
    explicit constexpr detached_t() noexcept = default;

    template <typename... Args>
    constexpr void operator()(Args &&...) const noexcept {};
};

inline constexpr auto detached = detached_t {};

}

#endif