#ifndef TCX_NATIVE_PATH_HPP
#define TCX_NATIVE_PATH_HPP

namespace tcx {

#if defined(OXYGEN_INVOKED)
using native_path_char_type = std::filesystem::path::value_type;
#elif defined(_WIN32)
using native_path_char_type = wchar_t;
#else
using native_path_char_type = char;
#endif

}

#endif
