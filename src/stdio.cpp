module mcpplibs.os.stdio;

import std;
import openkal.types;
import openkal.stream;
import openkal.timeout;
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

result<std::optional<std::string>> read_input_for(std::chrono::milliseconds timeout, std::size_t at_most) {
    std::string out(at_most, '\0');
    const auto ns = static_cast<kal_u64>(std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count());
    const kal_intptr n = kal_timeout_read(kal_stdin(), out.data(), out.size(), ns);
    if (n < 0) {
        const int code = static_cast<int>(-n);
        if (code == kal_err_again) return std::optional<std::string>{};
        if (code == kal_err_closed) return std::optional<std::string>{ std::string{} };
        return std::unexpected(from_kal(code, "reading standard input"));
    }
    out.resize(static_cast<std::size_t>(n));
    return std::optional<std::string>{ std::move(out) };
}

result<output_to_error> output_to_error::begin() {
    (void)flush_output();
    const int saved = sys::divert_output();
    if (saved < 0) return std::unexpected(error{ errc::not_supported, "sending standard output to standard error" });
    output_to_error d;
    d.saved_ = saved;
    return d;
}

output_to_error::output_to_error(output_to_error&& o) noexcept : saved_(std::exchange(o.saved_, -1)) {}
output_to_error& output_to_error::operator=(output_to_error&& o) noexcept {
    if (this != &o) {
        if (saved_ >= 0) sys::restore_output(saved_);
        saved_ = std::exchange(o.saved_, -1);
    }
    return *this;
}
output_to_error::~output_to_error() {
    if (saved_ < 0) return;
    (void)flush_output();
    sys::restore_output(saved_);
}

result<void> output_to_error::write_original(std::string_view bytes) {
    if (saved_ < 0) return write_output(bytes);
    if (!sys::write_descriptor(saved_, bytes)) return std::unexpected(error{ errc::io, "writing standard output" });
    return {};
}

}  // namespace mcpplibs::os::stdio
