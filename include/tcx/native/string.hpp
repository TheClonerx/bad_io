#ifndef TCX_NATIVE_PATH_HPP
#define TCX_NATIVE_PATH_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace tcx::native {

using path = std::filesystem::path;
using string = path::string_type;
using c_string = string::value_type const *;
using string_view = std::basic_string<string::value_type>;

} // namespace tcx::native

#endif
