// mcpplibs.os.text (0.4.6) --- the text another program writes, as UTF-8.
// Everywhere: UTF-8 passes unchanged and each sequence that is not UTF-8
// becomes one U+FFFD per maximal subpart. On Windows: the bytes cl.exe and
// link.exe wrote under consoles in code pages 936, 1252 and 437
// (mcpp-community/mcpp#970, probe run 44508574496) read as the text they are,
// and `console' is the page of the console this program is attached to.
import std;
import mcpplibs.os;
import mcpplibs.mtest;

namespace os = mcpplibs::os;
namespace text = mcpplibs::os::text;
using namespace mcpplibs::mtest;

namespace {
constexpr std::string_view fffd = "\xEF\xBF\xBD";
std::string repeat(std::string_view s, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += s;
    return out;
}
}  // namespace

UTest text_tests{"text", [] {
    "UTF-8 passes unchanged, in four scripts and beyond the basic plane"_test = [] {
        for (const std::string_view s : {"plain", "na\xC3\xAFve", "\xE6\x9C\xAA\xE5\xAE\x9A\xE4\xB9\x89_\xC3\xA9",
                                         "\xCE\x95\xCE\xBB", "\xF0\x9F\x98\x80"})
            expect(text::valid_utf8(s) == s) << s;
        expect(text::valid_utf8("").empty());
    };

    "each sequence that is not UTF-8 becomes U+FFFD, a maximal subpart at a time"_test = [] {
        // A lead no sequence begins with, and a lone continuation byte.
        expect(text::valid_utf8("a\xFF" "b") == "a" + std::string(fffd) + "b");
        expect(text::valid_utf8("\x80") == fffd);
        // A sequence cut short is one subpart, at the end and before ASCII.
        expect(text::valid_utf8("\xE6\x9C") == fffd);
        expect(text::valid_utf8("\xE6\x9C" "a") == std::string(fffd) + "a");
        // An overlong form and a surrogate: the lead's second byte is out of
        // its range, so each byte is its own subpart.
        expect(text::valid_utf8("\xC0\xAF") == repeat(fffd, 2));
        expect(text::valid_utf8("\xED\xA0\x80") == repeat(fffd, 3));
        expect(text::valid_utf8("\xF4\x90\x80\x80") == repeat(fffd, 4)) << "beyond U+10FFFF";
        // GBK read as UTF-8: what mcpp#970 handed a JSON writer.
        const auto gbk = text::valid_utf8("'\xCE\xB4\xB6\xA8\xD2\xE5_\xA8\xA6'");
        expect(text::valid_utf8(gbk) == gbk) << "the result is UTF-8";
    };

    "a decoder of UTF-8 is the replacement above"_test = [] {
        const text::decoder utf8(text::encoding::utf8);
        expect(utf8.code_page() == 65001u);
        expect(utf8("x\xFFy") == "x" + std::string(fffd) + "y");
    };

    if constexpr (os::is_windows) {
        "code pages 936, 1252 and 437 read as the text cl.exe wrote in them"_test = [] {
            // b.cpp(2): error C2065: '未定义_é' under a console in 936.
            expect(os::windows::utf8_from_code_page("'\xCE\xB4\xB6\xA8\xD2\xE5_\xA8\xA6'", 936)
                   == "'\xE6\x9C\xAA\xE5\xAE\x9A\xE4\xB9\x89_\xC3\xA9'");
            // The Chinese resources' prefix and quotes: 注意: 包含文件: and “缺失.h”.
            expect(os::windows::utf8_from_code_page("\xD7\xA2\xD2\xE2: \xB0\xFC\xBA\xAC\xCE\xC4\xBC\xFE:", 936)
                   == "\xE6\xB3\xA8\xE6\x84\x8F: \xE5\x8C\x85\xE5\x90\xAB\xE6\x96\x87\xE4\xBB\xB6:");
            expect(os::windows::utf8_from_code_page("\xA1\xB0\xC8\xB1\xCA\xA7.h\xA1\xB1", 936)
                   == "\xE2\x80\x9C\xE7\xBC\xBA\xE5\xA4\xB1.h\xE2\x80\x9D");
            expect(os::windows::utf8_from_code_page("???_\xE9", 1252) == "???_\xC3\xA9");
            expect(os::windows::utf8_from_code_page("???_\x82", 437) == "???_\xC3\xA9");
            expect(os::windows::utf8_from_code_page("", 936).empty());
            expect(os::windows::utf8_from_code_page("x", 12345).empty()) << "a page the system does not know";
        };

        "console is the page of the console this program is attached to, else the system's"_test = [] {
            const unsigned page = os::windows::console_text_code_page();
            expect(page != 0u);
            const text::decoder console(text::encoding::console);
            expect(console.code_page() == page);
            // A line of the probe in that page converts back to the text it was.
            const std::string sample = "b.cpp(2): error C2065: 'x': undeclared identifier";
            expect(console(sample) == sample) << "ASCII is ASCII in every page";
        };
    }
}};
