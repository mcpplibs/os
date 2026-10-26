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

}  // namespace mcpplibs::os::interrupt
