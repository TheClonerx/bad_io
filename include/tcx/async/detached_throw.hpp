#ifndef TCX_ASYNC_DETACHED_THROW_HPP
#define TCX_ASYNC_DETACHED_THROW_HPP

#include <system_error>

namespace tcx {

struct detached_throw_t {
    explicit constexpr detached_throw_t() noexcept = default;

    template <typename... Args>
    void operator()(std::error_code ec, Args &&...) const
    {
        if (ec)
            throw std::system_error(ec);
    };
};

inline constexpr auto detached_throw = detached_throw_t {};

}

#endif