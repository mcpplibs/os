module mcpplibs.os.stdio;

import std;
import openkal.types;
import openkal.stream;
import mcpplibs.os.error;

namespace mcpplibs::os::stdio {

namespace {
result<void> write_all(kal_stream s, std::string_view bytes, std::string_view doing) {
    // kal_stream_write moves the whole buffer or reports why not (clause 7.4).
    if (bytes.empty()) return {};
    const kal_intptr n = kal_stream_write(s, bytes.data(), bytes.size());
    if (n < 0) return std::unexpected(from_kal(static_cast<int>(-n), doing));
    if (static_cast<std::size_t>(n) != bytes.size()) return std::unexpected(error{ errc::io, std::string(doing) });
    return {};
}
}  // namespace

result<std::string> read_input(std::size_t at_most) {
    std::string out(at_most, '\0');
    const kal_intptr n = kal_stream_read(kal_stdin(), out.data(), out.size());
    if (n < 0) {
        if (-n == kal_err_closed) return std::string{};
        return std::unexpected(from_kal(static_cast<int>(-n), "reading standard input"));
    }
    out.resize(static_cast<std::size_t>(n));
    return out;
}

result<std::string> read_exactly(std::size_t n) {
    std::string out;
    out.reserve(n);
    while (out.size() < n) {
        auto c = read_input(n - out.size());
        if (!c) return std::unexpected(c.error());
        if (c->empty()) break;
        out += *c;
    }
    return out;
}

result<void> write_output(std::string_view bytes) { return write_all(kal_stdout(), bytes, "writing standard output"); }
result<void> write_error(std::string_view bytes) { return write_all(kal_stderr(), bytes, "writing standard error"); }

result<void> flush_output() {
    if (const int rc = kal_stream_flush(kal_stdout()); rc != kal_ok && rc != kal_err_not_supported)
        return std::unexpected(from_kal(rc, "flushing standard output"));
    return {};
}

}  // namespace mcpplibs::os::stdio
