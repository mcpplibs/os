// mcpplibs.os.library in an ok image: an image is linked whole, and openkal
// loads no library into it (SPEC clause 12), so there is nothing to open.
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
    return std::unexpected(error{errc::not_supported, "loading a library in an ok image", 0});
}

std::optional<handle> loaded(std::string_view) { return std::nullopt; }

void* process_symbol(std::string_view) { return nullptr; }

std::string_view suffix() noexcept { return ""; }

}  // namespace mcpplibs::os::library
