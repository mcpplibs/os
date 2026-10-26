module mcpplibs.os.error;

import std;

namespace mcpplibs::os {

std::string_view describe(errc code) noexcept {
    switch (code) {
        case errc::invalid:       return "invalid argument";
        case errc::again:         return "would block";
        case errc::io:            return "input/output error";
        case errc::no_memory:     return "out of memory";
        case errc::no_space:      return "no space left";
        case errc::permission:    return "permission denied";
        case errc::not_supported: return "not supported here";
        case errc::closed:        return "the other end is closed";
        case errc::not_found:     return "not found";
        case errc::exists:        return "already exists";
        case errc::not_empty:     return "directory not empty";
        case errc::is_directory:  return "is a directory";
        case errc::not_directory: return "not a directory";
        case errc::not_program:   return "not a program this system can start";
        case errc::timed_out:     return "timed out";
    }
    return "unknown condition";
}

error from_kal(int code, std::string_view context) {
    // openkal's values are 1..14 in the order errc lists them (types.h).
    const auto c = (code >= 1 && code <= 14) ? static_cast<errc>(code) : errc::io;
    return error{ c, std::string(context), code };
}

std::string error::message() const {
    return context.empty() ? std::string(describe(code))
                           : std::format("{}: {}", context, describe(code));
}

}  // namespace mcpplibs::os
