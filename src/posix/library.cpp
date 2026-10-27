// mcpplibs.os.library on Linux and macOS: the dynamic loader of the C library.
module;
#include <dlfcn.h>

module mcpplibs.os.library;

import std;
import mcpplibs.os.error;
import mcpplibs.os.fs;
import mcpplibs.os.platform;

namespace mcpplibs::os::library {

handle& handle::operator=(handle&& other) noexcept {
    if (this != &other) {
        if (owned_ && native_) ::dlclose(native_);
        native_ = std::exchange(other.native_, nullptr);
        owned_ = std::exchange(other.owned_, false);
    }
    return *this;
}

handle::~handle() {
    if (owned_ && native_) ::dlclose(native_);
}

void* handle::symbol(std::string_view name) const {
    if (!native_) return nullptr;
    const std::string n(name);
    return ::dlsym(native_, n.c_str());
}

bool supported() noexcept {
    // A static program's loader is a stub: it gives no handle, not even the program's own.
    static const bool answer = [] {
        void* self = ::dlopen(nullptr, RTLD_NOW);
        if (self) ::dlclose(self);
        return self != nullptr;
    }();
    return answer;
}

result<handle> open(const std::filesystem::path& file) {
    const std::string name = fs::utf8(file);
    if (!supported()) return std::unexpected(error{errc::not_supported, std::format("loading {}: the program is linked statically", name), 0});
    void* h = ::dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char* why = ::dlerror();
        return std::unexpected(error{errc::not_found, std::format("loading {}: {}", name, why ? why : "not loaded"), 0});
    }
    return handle(h, true);
}

std::optional<handle> loaded(std::string_view name) {
    const std::string n(name);
    void* h = ::dlopen(n.c_str(), RTLD_NOW | RTLD_NOLOAD);
    if (!h) return std::nullopt;
    // RTLD_NOLOAD counts a reference all the same: this handle gives it back.
    return handle(h, true);
}

void* process_symbol(std::string_view name) {
    const std::string n(name);
    return ::dlsym(RTLD_DEFAULT, n.c_str());
}

std::string_view suffix() noexcept { return is_macos ? ".dylib" : ".so"; }

}  // namespace mcpplibs::os::library
