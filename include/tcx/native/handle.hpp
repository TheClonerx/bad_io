#ifndef TCX_NATIVE_HANDLE_HPP
#define TCX_NATIVE_HANDLE_HPP

#include <cstdint>

namespace tcx {
#ifdef _WIN32
using native_handle_type = void *;
inline static native_handle_type invalid_handle = reinterpret_cast<void *>(~static_cast<std::uintptr_t>(0));
#else
using native_handle_type = int;
inline static native_handle_type invalid_handle = -1;
#endif

}

#endif