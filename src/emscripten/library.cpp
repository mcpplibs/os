// mcpplibs.os.library on Emscripten: a module loads another only when the
// program is linked as a main module (`-sMAIN_MODULE'), which is the
// program's decision at link and not this package's; linked as an ordinary
// program, as mcpp links one, there is nothing to open.
module mcpplibs.os.library;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::library {

handle& handle::operator=(handle&& other) noexcept {
    native_ = std::exchange(other.native_, nullptr);
    owned_ = std::exchange(other.owned_, false);
    return *this;
}

handle::~handle() = default;

void* handle::symbol(std::string_view) const { return nullptr; }

result<handle> open(const std::filesystem::path&) {
    return std::unexpected(error{errc::not_supported, "loading a library in a wasm module", 0});
}

std::optional<handle> loaded(std::string_view) { return std::nullopt; }

void* process_symbol(std::string_view) { return nullptr; }

bool supported() noexcept { return false; }

std::string_view suffix() noexcept { return ""; }

}  // namespace mcpplibs::os::library
