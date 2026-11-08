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
    // openkal reads a bound of zero as no bound; a zero timeout here asks
    // without waiting, as the reads of a started program's streams do.
    const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count();
    const auto ns = static_cast<kal_u64>(count < 1 ? 1 : count);
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

result<capture> capture::begin(stream which) {
    std::fflush(nullptr);
    (void)flush_output();
    capture c;
    c.descriptor_ = which == stream::output ? 1 : 2;
    c.file_ = std::filesystem::temp_directory_path()
        / std::format("mcpplibs-os-capture-{}-{:x}", c.descriptor_, std::random_device{}());
    c.saved_ = sys::divert_to_file(c.descriptor_, c.file_);
    if (c.saved_ < 0) {
        std::error_code ec;
        std::filesystem::remove(c.file_, ec);
        return std::unexpected(error{ errc::not_supported, "keeping what a standard stream is written" });
    }
    return c;
}

capture::capture(capture&& o) noexcept
    : descriptor_(std::exchange(o.descriptor_, -1)), saved_(std::exchange(o.saved_, -1)), file_(std::move(o.file_)) {}
capture& capture::operator=(capture&& o) noexcept {
    if (this != &o) {
        end();
        descriptor_ = std::exchange(o.descriptor_, -1);
        saved_ = std::exchange(o.saved_, -1);
        file_ = std::move(o.file_);
    }
    return *this;
}
capture::~capture() { end(); }

void capture::end() {
    if (saved_ < 0) return;
    std::fflush(nullptr);
    (void)flush_output();
    sys::restore_descriptor(descriptor_, std::exchange(saved_, -1));
}

std::string capture::text() {
    if (file_.empty()) return {};
    end();
    std::string kept;
    if (std::ifstream in(file_, std::ios::binary); in) kept.assign(std::istreambuf_iterator<char>(in), {});
    std::error_code ec;
    std::filesystem::remove(file_, ec);
    file_.clear();
    return kept;
}

result<void> output_to_error::write_original(std::string_view bytes) {
    if (saved_ < 0) return write_output(bytes);
    if (!sys::write_descriptor(saved_, bytes)) return std::unexpected(error{ errc::io, "writing standard output" });
    return {};
}

}  // namespace mcpplibs::os::stdio
