// The functions of libSystem this package's macOS units call, declared here
// rather than taken from the SDK's headers (as src/windows/win32.h does for
// Windows): the units then compile the same under the system's C library and
// under one that sits on openkal (openkal-musl, mcpp docs/24), whose headers
// are not the SDK's.
//
// EVERY NAME IS ONE NO C LIBRARY DEFINES, so it reaches libSystem whichever C
// library the program links; a name both define resolves to the program's own.
// What is a kernel call is made as one instead (src/macos/kernel.h).
//
// AND EACH MAY BE ABSENT FROM THE LINK. A build pointed at a target links
// against openkal-macos's stub of libSystem, which lists only the names
// openkal-macos calls; the manifest lets these be undefined there (`-U'), and
// the loader finds them in libSystem, which every program on this system loads.
// A link against the SDK binds them as usual.
#pragma once

extern "C" {

int sysctlbyname(const char* name, void* value, unsigned long* length, void* replacement, unsigned long size);

// Mach: the host's port, its page size and its memory statistics (HOST_VM_INFO64 = 4).
unsigned int mach_host_self(void);
int host_page_size(unsigned int host, unsigned long* size);
int host_statistics64(unsigned int host, int flavor, int* info, unsigned int* count);

// `vm_statistics64_data_t' (mach/vm_statistics.h): 38 words.
struct okos_vm_statistics64 {
    unsigned int       free_count, active_count, inactive_count, wire_count;
    unsigned long long zero_fill_count, reactivations, pageins, pageouts, faults, cow_faults, lookups, hits, purges;
    unsigned int       purgeable_count, speculative_count;
    unsigned long long decompressions, compressions, swapins, swapouts;
    unsigned int       compressor_page_count, throttled_count, external_page_count, internal_page_count;
    unsigned long long total_uncompressed_pages_in_compressor;
};

}
