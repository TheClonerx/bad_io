#ifndef TCX_IS_SERVICE_HPP
#define TCX_IS_SERVICE_HPP

#include <type_traits>

namespace tcx {
namespace impl {
    template <typename S>
    concept has_poll = requires(S &s)
    {
        { s.poll() };
    };
}

template <typename S>
struct is_service : std::bool_constant<std::is_destructible_v<S> && tcx::impl::has_poll<S>> {
};

template <typename S>
inline constexpr bool is_service_v = is_service<S>::value;

template <typename S>
concept service = is_service_v<S>;

}

#endif