// mcpplibs.os.interrupt --- a request to end this program, and ending as asked.
//
// openkal's stop word (`kal_process_stop_requested'): taken over on first use,
// read whenever the program chooses, waited upon with a bound, and never a
// handler that interrupts what is running (part 2 §6.4). A program that was
// asked to end finishes what it does and then ends as the request would have
// ended it, so a shell sees 130 or 143 and a waiting starter sees an end by
// request (openkal 0.17, `kal_process_end_as_requested').
export module mcpplibs.os.interrupt;

import std;

export namespace mcpplibs::os::interrupt {

// Whether this environment delivers a request to end as a notice at all.
bool available();

// Whether a request has arrived. The first call takes the request over.
bool requested();

// Waits until a request arrives, or until `timeout' passes; whether one did.
bool wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

// Ends this program as the request ended it; returns only when there was no
// request, and then does nothing.
void end_as_requested();

// Ends this program as a system ends one whose output's reader is gone: by the
// signal for it where the system has one (SIGPIPE, which a shell reports as
// 141). Returns where the system has none: there a write to a closed stream is
// refused and the program goes on (0.2.0). openkal reports a closed reader
// instead of ending the program, so a program that wants the system's ending
// asks for it after it has finished in order.
void end_as_output_closed();

}  // namespace mcpplibs::os::interrupt

namespace mcpplibs::os::interrupt::sys {
void end_as_output_closed();
}  // namespace mcpplibs::os::interrupt::sys
