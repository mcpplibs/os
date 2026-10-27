module;
#include "win32.h"

module mcpplibs.os.windows;

import std;
import mcpplibs.os.fs;

namespace mcpplibs::os::windows {

bool elevated() {
    using open_token = okos_bool (__stdcall*)(okos_handle, okos_dword, okos_handle*);
    using token_information = okos_bool (__stdcall*)(okos_handle, int, void*, okos_dword, okos_dword*);
    const okos_handle advapi = LoadLibraryW(L"advapi32.dll");
    if (!advapi) return false;
    const auto open = reinterpret_cast<open_token>(GetProcAddress(advapi, "OpenProcessToken"));
    const auto query = reinterpret_cast<token_information>(GetProcAddress(advapi, "GetTokenInformation"));
    if (!open || !query) return false;
    okos_handle token = nullptr;
    if (!open(GetCurrentProcess(), 0x0008 /* TOKEN_QUERY */, &token)) return false;
    okos_token_elevation e{};
    okos_dword got = 0;
    const bool ok = query(token, 20 /* TokenElevation */, &e, sizeof e, &got) != 0;
    CloseHandle(token);
    return ok && e.elevated != 0;
}

std::optional<bool> on_dev_drive(const std::filesystem::path& directory) {
    const std::string utf8 = fs::utf8(directory);
    const int n = MultiByteToWideChar(65001, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) return std::nullopt;
    std::wstring wide(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(65001, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), n);
    const okos_handle h = CreateFileW(wide.c_str(), 0x0080 /* FILE_READ_ATTRIBUTES */, 0x7, nullptr,
                                      3 /* OPEN_EXISTING */, 0x02000000 /* BACKUP_SEMANTICS */, nullptr);
    if (h == reinterpret_cast<okos_handle>(static_cast<std::intptr_t>(-1)) || h == nullptr) return std::nullopt;
    okos_persistent_volume info{};
    info.version = 1;
    info.flag_mask = 0x00002000;                  // PERSISTENT_VOLUME_STATE_DEV_VOLUME
    okos_dword returned = 0;
    const bool asked = DeviceIoControl(h, 0x0009023C /* FSCTL_QUERY_PERSISTENT_VOLUME_STATE */,
                                       &info, sizeof info, &info, sizeof info, &returned, nullptr) != 0;
    CloseHandle(h);
    if (!asked) return std::nullopt;
    return (info.volume_flags & 0x00002000) != 0;
}

}  // namespace mcpplibs::os::windows
