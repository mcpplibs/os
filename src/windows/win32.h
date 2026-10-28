// The Windows functions this package's Windows units call, declared here rather
// than taken from <windows.h> (part 2 §6.2): the units then compile the same
// under the MSVC ABI and under any C environment, and what they reach of the
// system is this list.
//
// THE TYPES ARE THE SYSTEM'S AND NOT THE C ENVIRONMENT'S. A C library that
// presents POSIX on Windows (openkal-musl, mcpp docs/24) brings the LP64 data
// model and a 32-bit `wchar_t', so `unsigned long' and `wchar_t' are not
// DWORD and WCHAR there. A DWORD is written `okos_dword' (32 bits) and a
// WCHAR `char16_t' (UTF-16) in every environment.
//
// TWO KINDS OF BINDING. The names declared `dllimport' are bound when the
// program is linked, and they are names every program over openkal links:
// openkal-windows calls each of them itself, so the import library of any
// toolchain --- the system's, or the one openkal-windows generates from its own
// list where the system's is absent --- carries them. The names in `late' are
// kernel32's too, and are found when first called (LoadLibraryW and
// GetProcAddress), so that a toolchain without the system's import libraries
// links this package unchanged. advapi32 is reached the same way, so that a
// program links nothing beyond kernel32 for a question few programs ask.
#pragma once

extern "C" {
using okos_bool = int;
using okos_dword = unsigned int;
using okos_handle = void*;
using okos_wchar = char16_t;

struct okos_memory_status {
    okos_dword length, load;
    unsigned long long total_physical, available_physical, total_page_file,
        available_page_file, total_virtual, available_virtual, available_extended_virtual;
};
struct okos_persistent_volume {
    okos_dword volume_flags, flag_mask, version, reserved;
};
struct okos_token_elevation { okos_dword elevated; };

__declspec(dllimport) okos_handle __stdcall CreateFileW(const okos_wchar*, okos_dword, okos_dword, void*, okos_dword, okos_dword, okos_handle);
__declspec(dllimport) okos_bool __stdcall DeviceIoControl(okos_handle, okos_dword, void*, okos_dword, void*, okos_dword, okos_dword*, void*);
__declspec(dllimport) okos_bool __stdcall CloseHandle(okos_handle);
__declspec(dllimport) okos_handle __stdcall GetCurrentProcess(void);
__declspec(dllimport) okos_handle __stdcall LoadLibraryW(const okos_wchar*);
__declspec(dllimport) void* __stdcall GetProcAddress(okos_handle, const char*);
__declspec(dllimport) okos_dword __stdcall GetLastError(void);
__declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned, okos_dword, const char*, int, okos_wchar*, int);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned, okos_dword, const okos_wchar*, int, char*, int,
                                                        const char*, okos_bool*);
__declspec(dllimport) okos_bool __stdcall FlushFileBuffers(okos_handle);
__declspec(dllimport) void* __stdcall LocalFree(void*);
__declspec(dllimport) okos_bool __stdcall GetConsoleMode(okos_handle, okos_dword*);
__declspec(dllimport) okos_dword __stdcall GetCurrentProcessId(void);
__declspec(dllimport) okos_bool __stdcall SetHandleInformation(okos_handle, okos_dword, okos_dword);
// 0.2.1: the process's standard handles (STD_OUTPUT_HANDLE = -11, STD_ERROR_HANDLE = -12).
__declspec(dllimport) okos_handle __stdcall GetStdHandle(okos_dword);
__declspec(dllimport) okos_bool __stdcall GetHandleInformation(okos_handle, okos_dword*);
}

namespace late {

template <class F>
F find(const char* name) {
    return reinterpret_cast<F>(GetProcAddress(LoadLibraryW(u"kernel32.dll"), name));
}

inline okos_bool GlobalMemoryStatusEx(okos_memory_status* a0) {
    static const auto f = find<okos_bool (__stdcall*)(okos_memory_status*)>("GlobalMemoryStatusEx");
    return f(a0);
}
inline okos_bool IsWow64Process2(okos_handle a0, unsigned short* a1, unsigned short* a2) {
    static const auto f = find<okos_bool (__stdcall*)(okos_handle, unsigned short*, unsigned short*)>("IsWow64Process2");
    return f(a0, a1, a2);
}
inline okos_bool FreeLibrary(okos_handle a0) {
    static const auto f = find<okos_bool (__stdcall*)(okos_handle)>("FreeLibrary");
    return f(a0);
}
inline okos_handle GetModuleHandleW(const okos_wchar* a0) {
    static const auto f = find<okos_handle (__stdcall*)(const okos_wchar*)>("GetModuleHandleW");
    return f(a0);
}
inline okos_bool K32EnumProcessModules(okos_handle a0, okos_handle* a1, okos_dword a2, okos_dword* a3) {
    static const auto f = find<okos_bool (__stdcall*)(okos_handle, okos_handle*, okos_dword, okos_dword*)>("K32EnumProcessModules");
    return f(a0, a1, a2, a3);
}
inline okos_bool GetVolumePathNameW(const okos_wchar* a0, okos_wchar* a1, okos_dword a2) {
    static const auto f = find<okos_bool (__stdcall*)(const okos_wchar*, okos_wchar*, okos_dword)>("GetVolumePathNameW");
    return f(a0, a1, a2);
}
inline okos_bool GetVolumeInformationW(const okos_wchar* a0, okos_wchar* a1, okos_dword a2, okos_dword* a3, okos_dword* a4, okos_dword* a5, okos_wchar* a6, okos_dword a7) {
    static const auto f = find<okos_bool (__stdcall*)(const okos_wchar*, okos_wchar*, okos_dword, okos_dword*, okos_dword*, okos_dword*, okos_wchar*, okos_dword)>("GetVolumeInformationW");
    return f(a0, a1, a2, a3, a4, a5, a6, a7);
}
inline okos_dword GetActiveProcessorCount(unsigned short a0) {
    static const auto f = find<okos_dword (__stdcall*)(unsigned short)>("GetActiveProcessorCount");
    return f(a0);
}
inline okos_bool GetLogicalProcessorInformationEx(int a0, void* a1, okos_dword* a2) {
    static const auto f = find<okos_bool (__stdcall*)(int, void*, okos_dword*)>("GetLogicalProcessorInformationEx");
    return f(a0, a1, a2);
}
inline unsigned GetACP() {
    static const auto f = find<unsigned (__stdcall*)()>("GetACP");
    return f();
}
inline unsigned GetConsoleOutputCP() {
    static const auto f = find<unsigned (__stdcall*)()>("GetConsoleOutputCP");
    return f();
}
inline unsigned GetDriveTypeW(const okos_wchar* a0) {
    static const auto f = find<unsigned (__stdcall*)(const okos_wchar*)>("GetDriveTypeW");
    return f(a0);
}
inline okos_dword GetFileAttributesW(const okos_wchar* a0) {
    static const auto f = find<okos_dword (__stdcall*)(const okos_wchar*)>("GetFileAttributesW");
    return f(a0);
}
inline unsigned long long GetTickCount64() {
    static const auto f = find<unsigned long long (__stdcall*)()>("GetTickCount64");
    return f();
}
inline okos_bool SetStdHandle(okos_dword a0, okos_handle a1) {
    static const auto f = find<okos_bool (__stdcall*)(okos_dword, okos_handle)>("SetStdHandle");
    return f(a0, a1);
}

}  // namespace late
