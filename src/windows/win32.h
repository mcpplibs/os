// The Windows functions this package's Windows units call, declared here rather
// than taken from <windows.h> (part 2 §6.2): the units then compile the same
// under the MSVC ABI and under any C environment, and what they reach of the
// system is this list.
#pragma once

extern "C" {
using okos_bool = int;
using okos_dword = unsigned long;
using okos_handle = void*;

struct okos_memory_status {
    okos_dword length, load;
    unsigned long long total_physical, available_physical, total_page_file,
        available_page_file, total_virtual, available_virtual, available_extended_virtual;
};
struct okos_persistent_volume {
    okos_dword volume_flags, flag_mask, version, reserved;
};
struct okos_token_elevation { okos_dword elevated; };

__declspec(dllimport) okos_bool __stdcall GlobalMemoryStatusEx(okos_memory_status*);
__declspec(dllimport) okos_handle __stdcall CreateFileW(const wchar_t*, okos_dword, okos_dword, void*, okos_dword, okos_dword, okos_handle);
__declspec(dllimport) okos_bool __stdcall DeviceIoControl(okos_handle, okos_dword, void*, okos_dword, void*, okos_dword, okos_dword*, void*);
__declspec(dllimport) okos_bool __stdcall CloseHandle(okos_handle);
__declspec(dllimport) okos_handle __stdcall GetCurrentProcess(void);
__declspec(dllimport) okos_bool __stdcall IsWow64Process2(okos_handle, unsigned short*, unsigned short*);
// advapi32 is reached at run time, so that a program linking this package
// links nothing beyond kernel32 for a question few programs ask.
__declspec(dllimport) okos_handle __stdcall LoadLibraryW(const wchar_t*);
__declspec(dllimport) void* __stdcall GetProcAddress(okos_handle, const char*);
// mcpplibs.os.library.
__declspec(dllimport) okos_bool __stdcall FreeLibrary(okos_handle);
__declspec(dllimport) okos_handle __stdcall GetModuleHandleW(const wchar_t*);
__declspec(dllimport) okos_dword __stdcall GetLastError(void);
__declspec(dllimport) okos_bool __stdcall K32EnumProcessModules(okos_handle, okos_handle*, okos_dword, okos_dword*);
__declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned, okos_dword, const char*, int, wchar_t*, int);
// 0.2.0
__declspec(dllimport) okos_dword __stdcall GetLastError(void);
__declspec(dllimport) okos_bool __stdcall FlushFileBuffers(okos_handle);
__declspec(dllimport) okos_bool __stdcall GetVolumePathNameW(const wchar_t*, wchar_t*, okos_dword);
__declspec(dllimport) okos_bool __stdcall GetVolumeInformationW(const wchar_t*, wchar_t*, okos_dword, okos_dword*,
                                                                okos_dword*, okos_dword*, wchar_t*, okos_dword);
__declspec(dllimport) void* __stdcall LocalFree(void*);
__declspec(dllimport) okos_dword __stdcall GetActiveProcessorCount(unsigned short);
__declspec(dllimport) okos_bool __stdcall GetLogicalProcessorInformationEx(int, void*, okos_dword*);
__declspec(dllimport) unsigned __stdcall GetACP(void);
__declspec(dllimport) unsigned __stdcall GetConsoleOutputCP(void);
__declspec(dllimport) okos_bool __stdcall GetConsoleMode(okos_handle, okos_dword*);
__declspec(dllimport) unsigned __stdcall GetDriveTypeW(const wchar_t*);
__declspec(dllimport) okos_dword __stdcall GetFileAttributesW(const wchar_t*);
__declspec(dllimport) okos_dword __stdcall GetCurrentProcessId(void);
__declspec(dllimport) unsigned long long __stdcall GetTickCount64(void);
__declspec(dllimport) okos_bool __stdcall SetHandleInformation(okos_handle, okos_dword, okos_dword);
// The C runtime's descriptors (every Windows C environment has these).
int __cdecl _dup(int);
int __cdecl _dup2(int, int);
int __cdecl _close(int);
long long __cdecl _get_osfhandle(int);
int __cdecl _write(int, const void*, unsigned);
}
