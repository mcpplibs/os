// No signal ends a program whose reader is gone here: its writes are refused.
module mcpplibs.os.interrupt;

namespace mcpplibs::os::interrupt::sys {
void end_as_output_closed() {}
}  // namespace mcpplibs::os::interrupt::sys
