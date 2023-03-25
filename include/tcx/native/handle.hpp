#ifndef TCX_NATIVE_HANDLE_HPP
#define TCX_NATIVE_HANDLE_HPP

namespace tcx::native {

#ifdef _DOXYGEN
/**
 * @brief native handle type used by the OS to identify a resource.
 */
using handle_type = /* implementation-defined */;
/**
 * @brief value used to represent an invalid handle
 * This value is constexpr if the implementation allows it.
 * @see handle_type
 */
inline /* constexpr */ static handle_type invalid_handle = /* implementation-defined */;
#endif

#ifdef _WIN32
using handle_type = void *;
inline static handle_type invalid_handle = (void *)-1;
#else
using handle_type = int;
inline constexpr static handle_type invalid_handle = -1;
#endif

} // namespace tcx::native

#endif
