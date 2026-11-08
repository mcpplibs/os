// The output descriptor saved (its handle not inherited) and replaced by the
// error one, under the Microsoft C runtime: `_dup2' to a standard descriptor
// also sets the process's standard handle, which openkal's standard output and
// a started program use. Selected where the C library is the system's (the
// manifest); src/windows/stdio.cpp serves a C library on openkal.
module;
#include "../win32.h"

extern "C" {
// The C runtime's descriptors.
int __cdecl _dup(int);
int __cdecl _dup2(int, int);
int __cdecl _close(int);
long long __cdecl _get_osfhandle(int);
int __cdecl _write(int, const void*, unsigned);
int __cdecl _wopen(const wchar_t*, int, ...);
}

module mcpplibs.os.stdio;

import std;

namespace mcpplibs::os::stdio::sys {

int divert_output() {
    const int saved = _dup(1);
    if (saved >= 0) {
        if (const long long h = _get_osfhandle(saved); h != -1)
            SetHandleInformation(reinterpret_cast<okos_handle>(h), 0x1 /* HANDLE_FLAG_INHERIT */, 0);
        _dup2(2, 1);
    }
    return saved;
}

void restore_output(int saved) {
    _dup2(saved, 1);
    _close(saved);
}

int divert_to_file(int descriptor, const std::filesystem::path& file) {
    // _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE
    const int target = _wopen(file.c_str(), 0x1 | 0x100 | 0x200 | 0x8000 | 0x80, 0x100 | 0x80);
    if (target < 0) return -1;
    const int saved = _dup(descriptor);
    if (saved < 0) {
        _close(target);
        return -1;
    }
    if (const long long h = _get_osfhandle(saved); h != -1)
        SetHandleInformation(reinterpret_cast<okos_handle>(h), 0x1 /* HANDLE_FLAG_INHERIT */, 0);
    _dup2(target, descriptor);   // also the process's standard handle
    _close(target);
    return saved;
}

void restore_descriptor(int descriptor, int saved) {
    _dup2(saved, descriptor);
    _close(saved);
}

bool write_descriptor(int descriptor, std::string_view bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const int n = _write(descriptor, bytes.data() + done, static_cast<unsigned>(bytes.size() - done));
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

}  // namespace mcpplibs::os::stdio::sys
