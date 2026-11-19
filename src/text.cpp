module mcpplibs.os.text;

import std;
import mcpplibs.os.platform;
import mcpplibs.os.windows;

namespace mcpplibs::os::text {

namespace {

// The first byte's sequence: its length and the range of its second byte, by
// Table 3-7 of The Unicode Standard; a length of 0 for a byte no well-formed
// sequence begins with.
struct lead {
    std::size_t length;
    unsigned char low, high;
};

lead lead_of(unsigned char b) {
    if (b >= 0xC2 && b <= 0xDF) return {2, 0x80, 0xBF};
    if (b == 0xE0) return {3, 0xA0, 0xBF};
    if (b == 0xED) return {3, 0x80, 0x9F};
    if (b >= 0xE1 && b <= 0xEF) return {3, 0x80, 0xBF};
    if (b == 0xF0) return {4, 0x90, 0xBF};
    if (b >= 0xF1 && b <= 0xF3) return {4, 0x80, 0xBF};
    if (b == 0xF4) return {4, 0x80, 0x8F};
    return {0, 0, 0};
}

constexpr std::string_view replacement = "\xEF\xBF\xBD";

}  // namespace

std::string valid_utf8(std::string_view bytes) {
    std::string out;
    out.reserve(bytes.size());
    std::size_t i = 0;
    while (i < bytes.size()) {
        const auto b = static_cast<unsigned char>(bytes[i]);
        if (b < 0x80) {
            out.push_back(static_cast<char>(b));
            ++i;
            continue;
        }
        const lead l = lead_of(b);
        // How many bytes from `i' belong to the sequence: the lead, a second
        // byte in its own range, then continuation bytes.
        std::size_t taken = 1;
        if (l.length != 0 && i + 1 < bytes.size()) {
            const auto second = static_cast<unsigned char>(bytes[i + 1]);
            if (second >= l.low && second <= l.high) taken = 2;
        }
        while (taken >= 2 && taken < l.length && i + taken < bytes.size()
               && (static_cast<unsigned char>(bytes[i + taken]) & 0xC0) == 0x80)
            ++taken;
        if (l.length != 0 && taken == l.length) out.append(bytes.substr(i, taken));
        else out.append(replacement);
        i += taken;
    }
    return out;
}

decoder::decoder(encoding from) {
    if constexpr (is_windows) {
        if (from == encoding::console) page_ = windows::console_text_code_page();
    }
}

std::string decoder::operator()(std::string_view bytes) const {
    if constexpr (is_windows) {
        if (page_ != 65001 && !bytes.empty()) {
            // A page the system does not know converts nothing: the bytes are
            // then read as UTF-8, the encoding of everything else here.
            if (std::string converted = windows::utf8_from_code_page(bytes, page_); !converted.empty())
                return converted;
        }
    }
    return valid_utf8(bytes);
}

}  // namespace mcpplibs::os::text
