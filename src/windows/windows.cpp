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

unsigned code_page() { return GetACP(); }

unsigned console_output_code_page(bool error_stream) {
    const long long h = _get_osfhandle(error_stream ? 2 : 1);
    okos_dword mode = 0;
    if (h == -1 || !GetConsoleMode(reinterpret_cast<okos_handle>(h), &mode)) return 0;
    return GetConsoleOutputCP();
}

namespace {
using reg_get_value = long (__stdcall*)(void*, const wchar_t*, const wchar_t*, okos_dword, okos_dword*, void*, okos_dword*);
reg_get_value registry_reader() {
    const okos_handle advapi = LoadLibraryW(L"advapi32.dll");
    return advapi ? reinterpret_cast<reg_get_value>(GetProcAddress(advapi, "RegGetValueW")) : nullptr;
}
void* const kLocalMachine = reinterpret_cast<void*>(static_cast<std::intptr_t>(static_cast<std::int32_t>(0x80000002)));
}  // namespace

bool long_paths_enabled() {
    const auto get = registry_reader();
    if (!get) return false;
    okos_dword value = 0, bytes = sizeof value;
    return get(kLocalMachine, L"SYSTEM\\CurrentControlSet\\Control\\FileSystem", L"LongPathsEnabled",
               0x10 /* RRF_RT_REG_DWORD */, nullptr, &value, &bytes) == 0
        && value == 1;
}

std::optional<std::filesystem::path> machine_registry_path(std::wstring_view key, std::wstring_view value) {
    const auto get = registry_reader();
    if (!get) return std::nullopt;
    const std::wstring k(key), v(value);
    for (const okos_dword view : { okos_dword{ 0x0100 } /* KEY_WOW64_64KEY */, okos_dword{ 0x0200 } /* 32KEY */ }) {
        okos_dword bytes = 0;
        if (get(kLocalMachine, k.c_str(), v.c_str(), 0x2 /* RRF_RT_REG_SZ */ | view, nullptr, nullptr, &bytes) != 0
            || bytes < sizeof(wchar_t))
            continue;
        std::wstring buf(bytes / sizeof(wchar_t), L'\0');
        if (get(kLocalMachine, k.c_str(), v.c_str(), 0x2 | view, nullptr, buf.data(), &bytes) != 0) continue;
        while (!buf.empty() && (buf.back() == L'\0' || buf.back() == L'\\')) buf.pop_back();
        if (!buf.empty()) return std::filesystem::path(buf);
    }
    return std::nullopt;
}

bool writable_fixed_drive(char drive, std::wstring_view child) {
    const wchar_t root[] = { static_cast<wchar_t>(drive), L':', L'\\', L'\0' };
    if (GetDriveTypeW(root) != 3 /* DRIVE_FIXED */) return false;
    const auto candidate = std::filesystem::path(root) / child;
    const auto attributes = GetFileAttributesW(candidate.c_str());
    if (attributes == 0xFFFFFFFFu) {
        const auto e = GetLastError();
        if (e != 2 /* FILE_NOT_FOUND */ && e != 3 /* PATH_NOT_FOUND */) return false;
    } else if (!(attributes & 0x10 /* DIRECTORY */) || (attributes & 0x400 /* REPARSE_POINT */)) {
        return false;
    }
    const auto probe = std::filesystem::path(root)
        / std::format(".os-write-{}-{}", GetCurrentProcessId(), GetTickCount64());
    const okos_handle h = CreateFileW(probe.c_str(), 0x40000000 /* GENERIC_WRITE */, 0, nullptr,
                                      1 /* CREATE_NEW */, 0x100 | 0x04000000 /* TEMPORARY | DELETE_ON_CLOSE */, nullptr);
    if (h == reinterpret_cast<okos_handle>(static_cast<std::intptr_t>(-1)) || h == nullptr) return false;
    CloseHandle(h);
    return true;
}

}  // namespace mcpplibs::os::windows
