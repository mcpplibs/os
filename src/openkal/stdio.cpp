// An image does not rearrange its streams.
module mcpplibs.os.stdio;

import std;

namespace mcpplibs::os::stdio::sys {
int divert_output() { return -1; }
void restore_output(int) {}
bool write_descriptor(int, std::string_view) { return false; }
}  // namespace mcpplibs::os::stdio::sys
