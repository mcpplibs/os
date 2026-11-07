// mcpplibs.os.posix in an ok image (0.4.0): the effective user, asked of the
// system the image runs upon. The creation mask is a state of the process that
// openkal does not expose, and remains undefined in an image.
module mcpplibs.os.posix;

import std;
import mcpplibs.os.platform;
import mcpplibs.os.process;

namespace mcpplibs::os::posix {

namespace {

// The second number of the `Uid:' line of /proc/self/status, the effective one.
std::optional<std::uint32_t> from_status() {
    std::ifstream in("/proc/self/status", std::ios::binary);
    for (std::string line; std::getline(in, line);) {
        if (!line.starts_with("Uid:")) continue;
        std::istringstream fields(line.substr(4));
        std::uint32_t real = 0, effective = 0;
        if (fields >> real >> effective) return effective;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> from_id() {
    process::spawn_options o;
    o.program = "/usr/bin/id";
    o.args = {"-u"};
    o.in = process::stdio::null;
    o.err = process::stdio::null;
    process::bounds limits;
    limits.hard = std::chrono::seconds{10};
    auto r = process::run(std::move(o), limits);
    if (!r || !r->status.success()) return std::nullopt;
    std::uint32_t v = 0;
    const auto& s = r->out;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end == s.data()) return std::nullopt;
    return v;
}

}  // namespace

// Not known -- not a POSIX system, or neither source answered -- is the largest
// number, which names no user.
std::uint32_t effective_user() {
    static const std::uint32_t answer = [] {
        std::optional<std::uint32_t> v;
        if (host() == platform::linux) v = from_status();
        if (!v && (host() == platform::linux || host() == platform::macos)) v = from_id();
        return v.value_or(std::numeric_limits<std::uint32_t>::max());
    }();
    return answer;
}

}  // namespace mcpplibs::os::posix
