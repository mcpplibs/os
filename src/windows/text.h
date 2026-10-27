// UTF-16, the system's text, from the UTF-8 this package passes names in (R7),
// and back. Included after `import std' by the units that need it.
#pragma once

namespace {

inline std::u16string utf16_from_utf8(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(65001, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::u16string out(static_cast<std::size_t>(n > 0 ? n : 0), u'\0');
    if (n > 0) MultiByteToWideChar(65001, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

inline std::u16string utf16_from_path(const std::filesystem::path& name) { return utf16_from_utf8(mcpplibs::os::fs::utf8(name)); }

// A `wchar_t' string is UTF-16 under the system's C environment and UTF-32
// under one that presents POSIX.
inline std::u16string utf16_from_wide(std::wstring_view wide) {
    if constexpr (sizeof(wchar_t) == 2) {
        return std::u16string(wide.begin(), wide.end());
    } else {
        std::u16string out;
        for (const wchar_t w : wide) {
            const auto c = static_cast<char32_t>(w);
            if (c < 0x10000) {
                out.push_back(static_cast<char16_t>(c));
            } else {
                out.push_back(static_cast<char16_t>(0xD800 + ((c - 0x10000) >> 10)));
                out.push_back(static_cast<char16_t>(0xDC00 + ((c - 0x10000) & 0x3FF)));
            }
        }
        return out;
    }
}

inline std::string utf8_from_utf16(std::u16string_view text) {
    if (text.empty()) return {};
    const int n = WideCharToMultiByte(65001, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(65001, 0, text.data(), static_cast<int>(text.size()), out.data(), n, nullptr, nullptr);
    return out;
}

// A name the system returned, as a path.
inline std::filesystem::path path_from_utf16(std::u16string_view text) { return mcpplibs::os::fs::from_utf8(utf8_from_utf16(text)); }

}  // namespace
