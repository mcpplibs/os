// mcpplibs.os.macos --- what macOS has and the others do not (R5).
export module mcpplibs.os.macos;

import std;

export namespace mcpplibs::os::macos {

// Whether this program is translated (Rosetta). A program built for one
// architecture and run on another has no portable way to ask, and the answer
// matters only here.
bool translated();

}  // namespace mcpplibs::os::macos
