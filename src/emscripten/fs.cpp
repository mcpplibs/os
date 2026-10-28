// Emscripten's own of mcpplibs.os.fs: nothing is exchanged (the C library has
// no renameat2), and the kind of a volume is the file system Emscripten
// mounted there, asked of its FS (statfs answers the same constants for every
// one). Durability, ownership and whether a node may be written are the C
// library's, in src/posix/fs.cpp.
module;
#include <emscripten/em_asm.h>

module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

int exchange(const std::filesystem::path&, const std::filesystem::path&) { return -1; }
bool exchange_unsupported(int c) { return c == -1; }

// `memfs', `nodefs', `idbfs', `workerfs', `noderawfs' or `opfs', by the mount
// the directory lies in; nothing where the FS cannot say (a name not found,
// or a file system this list does not name).
std::string volume_type(const std::filesystem::path& directory) {
    const std::string name = directory.generic_string();
    const int kind = EM_ASM_INT({
        try {
            var t = FS.lookupPath(UTF8ToString($0), { follow: true }).node.mount.type;
            if (typeof MEMFS !== 'undefined' && t === MEMFS) return 1;
            if (typeof NODEFS !== 'undefined' && t === NODEFS) return 2;
            if (typeof IDBFS !== 'undefined' && t === IDBFS) return 3;
            if (typeof WORKERFS !== 'undefined' && t === WORKERFS) return 4;
            if (typeof NODERAWFS !== 'undefined' && t === NODERAWFS) return 5;
            if (typeof OPFS !== 'undefined' && t === OPFS) return 6;
        } catch (e) {}
        return 0;
    }, name.c_str());
    switch (kind) {
        case 1: return "memfs";
        case 2: return "nodefs";
        case 3: return "idbfs";
        case 4: return "workerfs";
        case 5: return "noderawfs";
        case 6: return "opfs";
    }
    return {};
}

}  // namespace mcpplibs::os::fs::sys
