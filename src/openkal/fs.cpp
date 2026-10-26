// An image's own of mcpplibs.os.fs: what openkal answers. Durability is the
// stream's flush where the node is a file; nothing is exchanged; the kind and
// the owner of a volume are not known to an image.
module;

module mcpplibs.os.fs;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::fs::sys {

result<void> sync(const std::filesystem::path&) { return {}; }
int exchange(const std::filesystem::path&, const std::filesystem::path&) { return -1; }
bool exchange_unsupported(int c) { return c == -1; }
std::string volume_type(const std::filesystem::path&) { return {}; }
owner owner_of(const std::filesystem::path&) { return owner::unknown; }
bool writable(const std::filesystem::path&) { return true; }

}  // namespace mcpplibs::os::fs::sys
