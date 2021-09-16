#ifndef TCX_IS_SERVICE_HPP
#define TCX_IS_SERVICE_HPP

#include <type_traits>

namespace tcx {
namespace impl {
    template <typename S, typename E>
    concept has_poll = requires(S &s, E &e)
    {
        { s.poll(e) };
    };
}

template <typename S, typename E>
struct is_service : std::bool_constant<std::is_destructible_v<S> && std::is_move_constructible_v<S> && std::is_move_assignable_v<S> && tcx::impl::has_poll<S, E>> {
};

template <typename S, typename E>
inline constexpr bool is_service_v = is_service<S, E>::value;

template <typename S, typename E>
concept service = is_service_v<S, E>;

}

#endif