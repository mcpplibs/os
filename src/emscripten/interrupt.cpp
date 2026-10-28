// mcpplibs.os.interrupt on Emscripten: no request to end reaches a wasm
// module as a notice (openkal-emscripten provides no `openkal.process', whose
// stop word it would be), so none is available and none arrives; and no
// signal ends a program whose reader is gone: its writes are refused.
module mcpplibs.os.interrupt;

import std;

namespace mcpplibs::os::interrupt {

bool available() { return false; }

bool requested() { return false; }

bool wait(std::optional<std::chrono::milliseconds> timeout) {
    if (timeout) std::this_thread::sleep_for(*timeout);
    return false;
}

void end_as_requested() {}

void end_as_output_closed() { sys::end_as_output_closed(); }

}  // namespace mcpplibs::os::interrupt

namespace mcpplibs::os::interrupt::sys {
void end_as_output_closed() {}
}  // namespace mcpplibs::os::interrupt::sys
