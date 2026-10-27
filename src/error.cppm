// mcpplibs.os.error --- what went wrong, in a portable classification.
//
// The classification is openkal's closed set of conditions (SPEC clause 5.2),
// so a program acts upon the same value on every system; the system's own
// code is kept beside it for a report, and so is what was being done.
export module mcpplibs.os.error;

import std;

export namespace mcpplibs::os {

enum class errc {
    invalid = 1, again, io, no_memory, no_space, permission, not_supported, closed,
    not_found, exists, not_empty, is_directory, not_directory, not_program,
    timed_out,
    too_large,   // more than the caller would take (0.2.1)
};

struct error {
    errc        code = errc::io;
    std::string context;          // what was being done, and to what
    int         native = 0;       // the kernel layer's value, for a report

    // `context: condition', for a message.
    std::string message() const;
};

template <class T>
using result = std::expected<T, error>;

// The condition an openkal operation reported, with what was being done.
error from_kal(int code, std::string_view context);

// The text of a condition.
std::string_view describe(errc code) noexcept;

}  // namespace mcpplibs::os
