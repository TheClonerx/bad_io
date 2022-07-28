#ifndef TCX_NATIVE_HANDLE_HPP
#define TCX_NATIVE_HANDLE_HPP

#include <cstdint>

namespace tcx::native {

#ifdef _WIN32
using handle_type = void *;
inline static handle_type invalid_handle = reinterpret_cast<void *>(~static_cast<std::uintptr_t>(0));
#else
using handle_type = int;
inline static handle_type invalid_handle = -1;
#endif

} // namespace tcx::native

#endif
