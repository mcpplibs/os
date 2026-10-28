// Emscripten starts no program and replaces no program's image.
module mcpplibs.os.process;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::process::sys {

result<void> replace_image(const std::filesystem::path&, const std::vector<std::string>&,
                           const std::vector<std::string>&, const std::filesystem::path&) {
    return std::unexpected(error{ errc::not_supported, "this system replaces no program's image" });
}

}  // namespace mcpplibs::os::process::sys
