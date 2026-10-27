// Windows's own of mcpplibs.os.fs: durability (FlushFileBuffers), the volume's
// kind, the owner of a node (advapi32, reached at run time); no exchange.
module;
#include "win32.h"

module mcpplibs.os.fs;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::fs::sys {

namespace {
constexpr okos_dword kNotSupported = 50, kInvalidFunction = 1, kFileNotFound = 2;
okos_handle invalid() { return reinterpret_cast<okos_handle>(static_cast<std::intptr_t>(-1)); }
}  // namespace

result<void> sync(const std::filesystem::path& node) {
    std::error_code ec;
    if (std::filesystem::is_directory(node, ec)) return {};   // nothing of a directory is flushed here
    const okos_handle h = CreateFileW(node.c_str(), 0x40000000 /* GENERIC_WRITE */, 0x7 /* share all */,
                                      nullptr, 3 /* OPEN_EXISTING */, 0x80 /* NORMAL */, nullptr);
    if (h == invalid() || h == nullptr) {
        const auto e = GetLastError();
        return std::unexpected(error{ e == kFileNotFound ? errc::not_found : errc::io,
            std::format("making {} durable: {}", mcpplibs::os::fs::generic_utf8(node), std::system_category().message(static_cast<int>(e))),
            static_cast<int>(e) });
    }
    const bool ok = FlushFileBuffers(h) != 0;
    const auto e = GetLastError();
    CloseHandle(h);
    if (!ok) return std::unexpected(error{ errc::io, std::format("making {} durable: {}", mcpplibs::os::fs::generic_utf8(node),
                                                                 std::system_category().message(static_cast<int>(e))),
                                           static_cast<int>(e) });
    return {};
}

int exchange(const std::filesystem::path&, const std::filesystem::path&) { return static_cast<int>(kNotSupported); }
bool exchange_unsupported(int c) {
    return c == static_cast<int>(kNotSupported) || c == static_cast<int>(kInvalidFunction);
}

std::string volume_type(const std::filesystem::path& directory) {
    wchar_t root[260] {}, name[260] {};
    if (!GetVolumePathNameW(directory.c_str(), root, 260)) return {};
    if (!GetVolumeInformationW(root, nullptr, 0, nullptr, nullptr, nullptr, name, 260)) return {};
    return mcpplibs::os::fs::utf8(std::filesystem::path(name));
}

owner owner_of(const std::filesystem::path& node) {
    using named_security = okos_dword (__stdcall*)(const wchar_t*, int, okos_dword, void**, void**, void**, void**, void**);
    using well_known = okos_bool (__stdcall*)(void*, int);
    using open_token = okos_bool (__stdcall*)(okos_handle, okos_dword, okos_handle*);
    using token_information = okos_bool (__stdcall*)(okos_handle, int, void*, okos_dword, okos_dword*);
    using equal_sid = okos_bool (__stdcall*)(void*, void*);
    const okos_handle advapi = LoadLibraryW(L"advapi32.dll");
    if (!advapi) return owner::unknown;
    const auto security = reinterpret_cast<named_security>(GetProcAddress(advapi, "GetNamedSecurityInfoW"));
    const auto known = reinterpret_cast<well_known>(GetProcAddress(advapi, "IsWellKnownSid"));
    const auto open = reinterpret_cast<open_token>(GetProcAddress(advapi, "OpenProcessToken"));
    const auto query = reinterpret_cast<token_information>(GetProcAddress(advapi, "GetTokenInformation"));
    const auto equal = reinterpret_cast<equal_sid>(GetProcAddress(advapi, "EqualSid"));
    if (!security || !known || !open || !query || !equal) return owner::unknown;
    void* who = nullptr;
    void* descriptor = nullptr;
    if (security(node.c_str(), 1 /* SE_FILE_OBJECT */, 0x1 /* OWNER_SECURITY_INFORMATION */,
                 &who, nullptr, nullptr, nullptr, &descriptor) != 0)
        return owner::unknown;
    owner found = owner::another_user;
    if (known(who, 22 /* WinLocalSystemSid */) || known(who, 26 /* WinBuiltinAdministratorsSid */)) {
        found = owner::system;
    } else {
        okos_handle token = nullptr;
        if (open(GetCurrentProcess(), 0x0008 /* TOKEN_QUERY */, &token)) {
            okos_dword size = 0;
            query(token, 1 /* TokenUser */, nullptr, 0, &size);
            std::vector<unsigned char> buffer(size);
            // TOKEN_USER begins with the user's SID_AND_ATTRIBUTES, whose first member is the SID.
            if (size >= sizeof(void*) && query(token, 1, buffer.data(), size, &size)
                && equal(who, *reinterpret_cast<void**>(buffer.data())))
                found = owner::this_user;
            CloseHandle(token);
        }
    }
    LocalFree(descriptor);
    return found;
}

bool writable(const std::filesystem::path&) { return true; }

}  // namespace mcpplibs::os::fs::sys
