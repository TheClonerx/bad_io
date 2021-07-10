#ifndef TCX_UNIQUE_TYPES_HPP
#define TCX_UNIQUE_TYPES_HPP

#include <tuple>
#include <type_traits>

namespace tcx {

namespace impl {
    template <typename Tuple, typename... T>
    struct _add_type_if_not_present;

    template <typename... Ts>
    struct _add_type_if_not_present<std::tuple<Ts...>> {
        using type = std::tuple<>;
    };

    template <typename... Ts, typename U>
    struct _add_type_if_not_present<std::tuple<Ts...>, U> {
        using type = std::conditional_t<std::disjunction_v<std::is_same<Ts, U>...>, std::tuple<Ts...>, std::tuple<Ts..., U>>;
    };

    template <typename... Ts, typename First, typename... Rest>
    struct _add_type_if_not_present<std::tuple<Ts...>, First, Rest...> {
        using type = typename _add_type_if_not_present<typename _add_type_if_not_present<std::tuple<Ts...>, First>::type, Rest...>::type;
    };

}

template <typename... T>
struct unique_types {
    using type = typename impl::_add_type_if_not_present<std::tuple<>, T...>::type;
};

template <typename... T>
using unique_types_t = typename unique_types<T...>::type;

}

#endif