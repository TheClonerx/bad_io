#ifndef TCX_UTILITIES_CLAMP_HPP
#define TCX_UTILITIES_CLAMP_HPP

#include <algorithm>
#include <limits>
#include <type_traits>

#include <tcx/utilities/nonbool_integer.hpp>

namespace tcx::utilities {

/**
 * @brief clamps the value `integral` to the [`low`; `high`] range of the resulting integer type `Result`
 */
template <tcx::utilities::nonbool_integral Result, tcx::utilities::nonbool_integral I>
constexpr Result clamp(I integral, Result low, Result high) noexcept
{
    return static_cast<Result>(std::clamp<I>(integral, low, high));
}

/**
 * @brief clamps the value `integral` to the range of the resulting integer type `Result`
 */
template <tcx::utilities::nonbool_integral Result, tcx::utilities::nonbool_integral I>
constexpr Result clamp(I integral) noexcept
{
    return clamp(integral, std::numeric_limits<Result>::min(), std::numeric_limits<Result>::max());
}

} // namespace tcx::utilities

#endif
