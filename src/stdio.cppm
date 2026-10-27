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

// One chunk of input that arrives within `timeout'; nothing when none did,
// empty at the end of input (0.2.0). A zero timeout asks without waiting.
// errc::not_supported where the input cannot be read with a bound.
result<std::optional<std::string>> read_input_for(std::chrono::milliseconds timeout,
                                                  std::size_t at_most = 65536);

// THIS PROGRAM'S STANDARD OUTPUT IS ITS STANDARD ERROR UNTIL THIS IS DESTROYED
// (0.2.0): what it writes there, and what the programs it starts meanwhile
// write, reaches standard error, and `write_original' reaches the output as it
// was. A program whose output is a document narrates on standard error while
// it works and writes the document alone. The saved output is not inherited by
// the programs it starts. errc::not_supported where the streams cannot be
// rearranged (an image).
class output_to_error {
public:
    static result<output_to_error> begin();
    output_to_error() = default;
    output_to_error(output_to_error&&) noexcept;
    output_to_error& operator=(output_to_error&&) noexcept;
    output_to_error(const output_to_error&) = delete;
    output_to_error& operator=(const output_to_error&) = delete;
    ~output_to_error();
    result<void> write_original(std::string_view bytes);
private:
    int saved_ = -1;
};

}  // namespace mcpplibs::os::stdio

// Each system's own (src/<os>/stdio.cpp): the output descriptor saved and
// replaced by the error one (-1 when not possible), put back, and written.
namespace mcpplibs::os::stdio::sys {
int divert_output();
void restore_output(int saved);
bool write_descriptor(int descriptor, std::string_view bytes);
}  // namespace mcpplibs::os::stdio::sys
