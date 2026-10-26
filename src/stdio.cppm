// mcpplibs.os.stdio --- this program's own three streams, as bytes.
//
// openkal's streams are bytes on every system, so a protocol framed by
// lengths (a language server's Content-Length) reads and writes exactly what
// it means; nothing translates line ends. A write to a stream whose reader is
// gone is errc::closed, which a program reports or treats as the end of its
// output --- it is not ended by the system for it (part 2 §6.4).
export module mcpplibs.os.stdio;

import std;
export import mcpplibs.os.error;

export namespace mcpplibs::os::stdio {

// One chunk of input; empty at the end.
result<std::string> read_input(std::size_t at_most = 65536);
// Exactly `n' bytes, or fewer at the end of input.
result<std::string> read_exactly(std::size_t n);

result<void> write_output(std::string_view bytes);
result<void> write_error(std::string_view bytes);
result<void> flush_output();

}  // namespace mcpplibs::os::stdio
