// mcpplibs.os.library on Windows: the loader of kernel32 (src/windows/win32.h).
module;
#include "win32.h"

module mcpplibs.os.library;

import std;
import mcpplibs.os.error;
import mcpplibs.os.fs;

#include "text.h"

namespace mcpplibs::os::library {

handle& handle::operator=(handle&& other) noexcept {
    if (this != &other) {
        if (owned_ && native_) late::FreeLibrary(native_);
        native_ = std::exchange(other.native_, nullptr);
        owned_ = std::exchange(other.owned_, false);
    }
    return *this;
}

handle::~handle() {
    if (owned_ && native_) late::FreeLibrary(native_);
}

void* handle::symbol(std::string_view name) const {
    if (!native_) return nullptr;
    const std::string n(name);
    return GetProcAddress(native_, n.c_str());
}

result<handle> open(const std::filesystem::path& file) {
    const std::string name = fs::utf8(file);
    okos_handle h = LoadLibraryW(utf16_from_utf8(name).c_str());
    if (!h) return std::unexpected(error{errc::not_found, std::format("loading {}", name), static_cast<int>(GetLastError())});
    return handle(h, true);
}

std::optional<handle> loaded(std::string_view name) {
    // GetModuleHandleW counts no reference: the handle closes nothing.
    okos_handle h = late::GetModuleHandleW(utf16_from_utf8(name).c_str());
    if (!h) return std::nullopt;
    return handle(h, false);
}

void* process_symbol(std::string_view name) {
    const std::string n(name);
    okos_handle modules[1024];
    okos_dword bytes = 0;
    if (!late::K32EnumProcessModules(GetCurrentProcess(), modules, sizeof modules, &bytes)) return nullptr;
    const std::size_t count = std::min<std::size_t>(bytes / sizeof(okos_handle), std::size(modules));
    for (std::size_t i = 0; i < count; ++i)
        if (void* p = GetProcAddress(modules[i], n.c_str())) return p;
    return nullptr;
}

bool supported() noexcept { return true; }

std::string_view suffix() noexcept { return ".dll"; }

}  // namespace mcpplibs::os::library
