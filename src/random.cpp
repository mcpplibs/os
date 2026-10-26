module mcpplibs.os.random;

import std;
import openkal.types;
import openkal.random;
import mcpplibs.os.error;

namespace mcpplibs::os::random {

result<void> fill(std::span<std::byte> out) {
    if (out.empty()) return {};
    if (const int rc = kal_random_fill(out.data(), out.size()); rc != kal_ok)
        return std::unexpected(from_kal(rc, "reading the system's source of unpredictable bytes"));
    return {};
}

result<std::string> hex(std::size_t n) {
    std::vector<std::byte> bytes(n);
    if (auto r = fill(bytes); !r) return std::unexpected(r.error());
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (const auto b : bytes) {
        const auto v = std::to_integer<unsigned>(b);
        out.push_back(digits[v >> 4]);
        out.push_back(digits[v & 0xf]);
    }
    return out;
}

}  // namespace mcpplibs::os::random
