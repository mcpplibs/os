// mcpplibs.os.text --- the text another program writes, as UTF-8 (0.4.6).
//
// THIS PACKAGE'S TEXT IS UTF-8 (R7), AND SO IS A PROGRAM'S ON EVERY SYSTEM BUT
// ONE CONVENTION. A Windows console program writes its text to a pipe or a
// file in the output code page of the console it is attached to, and in the
// system's ANSI code page when it is attached to none. Measured with cl.exe and
// link.exe (mcpp-community/mcpp#970, windows-2022): under consoles in code pages
// 437, 1252, 936 and 65001 each wrote in the console's page, with no console in
// the system's (1252), and a character the page cannot represent arrived as
// `?'. A program that starts such a program and reads what it writes decodes
// the text here.
//
// THE PAGE IS DECIDED WHERE THE PROGRAM IS STARTED. A program inherits the
// console of the program that starts it, or the absence of one, so the page a
// decoder reads when it is made is the page the program it reads writes in.
//
//   const text::decoder decode(text::encoding::console);
//   ... started with its two streams in one pipe ...
//   for each line read: std::string utf8 = decode(line);
//
// A line is a whole unit: no code page of the system, and not UTF-8, puts the
// byte of a line end inside a character, so text decoded a line at a time is
// decoded correctly.
export module mcpplibs.os.text;

import std;

export namespace mcpplibs::os::text {

// How a program writes its text.
enum class encoding : std::uint8_t {
    utf8,     // UTF-8
    console,  // the convention above; UTF-8 on every system but Windows
};

// `bytes' with each sequence that is not UTF-8 replaced by U+FFFD, a maximal
// subpart at a time (The Unicode Standard, section 3.9): the result is UTF-8.
std::string valid_utf8(std::string_view bytes);

// The text of one program, as UTF-8.
class decoder {
public:
    explicit decoder(encoding from);
    // `bytes' as UTF-8: a sequence that is no character of the encoding
    // becomes a replacement character.
    [[nodiscard]] std::string operator()(std::string_view bytes) const;
    // The Windows code page the text is read in: 65001 for UTF-8, and on every
    // other system.
    [[nodiscard]] unsigned code_page() const noexcept { return page_; }

private:
    unsigned page_ = 65001;
};

}  // namespace mcpplibs::os::text
