#ifndef TCX_UTILITIES_NONBOOL_INTEGER_HPP
#define TCX_UTILITIES_NONBOOL_INTEGER_HPP

#include <type_traits>

namespace tcx::utilities {

template <typename T>
struct is_nonbool_integral : std::bool_constant<std::is_integral_v<T> && !std::is_same_v<std::remove_cvref_t<T>, bool>> { };
template <typename T>
inline constexpr bool is_nonbool_integral_v = is_nonbool_integral<T>::value;

template <typename T>
concept nonbool_integral = is_nonbool_integral_v<T>;

} // namespace tcx::utilities

#endif
