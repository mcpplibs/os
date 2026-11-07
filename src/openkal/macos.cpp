// mcpplibs.os.macos in an ok image (0.4.0): never translated. macOS runs
// images through an okrun of its own architecture only (aarch64; there is no
// x86-64 okrun for macOS), so an image there is never run by Rosetta. When an
// x86-64 okrun exists, the answer is "an x86-64 image on a machine with
// `hw.optional.arm64'"; `sysctl.proc_translated' would answer for the sysctl
// program asked, not for the image.
module mcpplibs.os.macos;

import std;

namespace mcpplibs::os::macos {

bool translated() { return false; }

}  // namespace mcpplibs::os::macos
