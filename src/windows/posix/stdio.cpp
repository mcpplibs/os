// The output diverted for a C library that presents POSIX on Windows over
// openkal (openkal-musl): its descriptor 1 is saved (closed on exec) and made
// one for the error output, and the process's standard output handle, which
// openkal's standard output and a started program use, is given the standard
// error's for as long, the handle it had kept not inherited meanwhile. The C
// library holds the handle its descriptor 1 began with, so neither change
// reaches what the other does. Selected by the manifest for such a library;
// src/windows/crt/stdio.cpp serves the Microsoft C runtime.
module;
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include "../win32.h"

module mcpplibs.os.stdio;

import std;

namespace mcpplibs::os::stdio::sys {

namespace {
constexpr okos_dword kOutput = static_cast<okos_dword>(-11), kError = static_cast<okos_dword>(-12);
constexpr okos_dword kInherit = 0x1;   // HANDLE_FLAG_INHERIT
struct kept { okos_handle handle; okos_dword flags; };
std::mutex guard;
std::vector<kept> given;   // the standard output handles to give back, innermost last
}  // namespace

int divert_output() {
    const int saved = ::fcntl(1, F_DUPFD_CLOEXEC, 3);
    if (saved < 0) return -1;
    ::dup2(2, 1);
    const std::lock_guard lock(guard);
    const okos_handle output = GetStdHandle(kOutput);
    okos_dword flags = 0;
    if (!GetHandleInformation(output, &flags)) flags = 0;
    given.push_back({ output, flags });
    SetHandleInformation(output, kInherit, 0);
    late::SetStdHandle(kOutput, GetStdHandle(kError));
    return saved;
}

void restore_output(int saved) {
    ::dup2(saved, 1);
    ::close(saved);
    const std::lock_guard lock(guard);
    if (given.empty()) return;
    const kept k = given.back();
    given.pop_back();
    late::SetStdHandle(kOutput, k.handle);
    SetHandleInformation(k.handle, kInherit, k.flags & kInherit);
}

// The C library's descriptor and the process's standard handle are made two
// writers of one file, each appending, so neither overwrites what the other wrote.
int divert_to_file(int descriptor, const std::filesystem::path& file) {
    const auto name = file.string();
    const int target = ::open(name.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_CLOEXEC, 0600);
    if (target < 0) return -1;
    std::u16string wide;
    for (unsigned char c : name) wide.push_back(static_cast<char16_t>(c));
    // FILE_APPEND_DATA | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING
    const okos_handle appended = CreateFileW(wide.c_str(), 0x4 | 0x100000, 0x1 | 0x2, nullptr, 3, 0x80, nullptr);
    const int saved = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 3);
    if (saved < 0 || appended == reinterpret_cast<okos_handle>(-1)) {
        if (saved >= 0) ::close(saved);
        if (appended != reinterpret_cast<okos_handle>(-1)) CloseHandle(appended);
        ::close(target);
        return -1;
    }
    ::dup2(target, descriptor);
    ::close(target);
    const okos_dword which = descriptor == 1 ? kOutput : kError;
    const std::lock_guard lock(guard);
    const okos_handle was = GetStdHandle(which);
    okos_dword flags = 0;
    if (!GetHandleInformation(was, &flags)) flags = 0;
    given.push_back({ was, flags });
    SetHandleInformation(was, kInherit, 0);
    late::SetStdHandle(which, appended);
    return saved;
}

void restore_descriptor(int descriptor, int saved) {
    ::dup2(saved, descriptor);
    ::close(saved);
    const okos_dword which = descriptor == 1 ? kOutput : kError;
    const std::lock_guard lock(guard);
    if (given.empty()) return;
    const kept k = given.back();
    given.pop_back();
    const okos_handle appended = GetStdHandle(which);
    late::SetStdHandle(which, k.handle);
    SetHandleInformation(k.handle, kInherit, k.flags & kInherit);
    if (appended != k.handle) CloseHandle(appended);
}

bool write_descriptor(int descriptor, std::string_view bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const auto n = ::write(descriptor, bytes.data() + done, bytes.size() - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

}  // namespace mcpplibs::os::stdio::sys
