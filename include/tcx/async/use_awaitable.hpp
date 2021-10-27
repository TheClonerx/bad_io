#ifndef TCX_ASYNC_USE_AWAITABLE_HPP
#define TCX_ASYNC_USE_AWAITABLE_HPP

namespace tcx {

struct use_awaitable_t {
    explicit constexpr use_awaitable_t() noexcept = default;
};

inline constexpr auto use_awaitable = use_awaitable_t {};

}

#endif