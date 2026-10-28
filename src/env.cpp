module mcpplibs.os.env;

import std;
import openkal.types;
import openkal.env;
import openkal.fs;
import mcpplibs.os.platform;
import mcpplibs.os.fs;

namespace mcpplibs::os::env {

namespace {
// A value of a counting operation: the length it has, then the bytes.
template <class F>
std::optional<std::string> counted(F f) {
    const kal_intptr n = f(nullptr, 0);
    if (n < 0) return std::nullopt;
    std::string out(static_cast<std::size_t>(n), '\0');
    if (n > 0) f(out.data(), out.size());
    return out;
}
}  // namespace

std::optional<std::string> get(std::string_view name) {
    return counted([&](char* out, kal_uintptr cap) { return kal_env_var(name.data(), name.size(), out, cap); });
}

std::vector<std::string> variables() {
    std::vector<std::string> out;
    const kal_uintptr n = kal_env_var_count();
    for (kal_uintptr i = 0; i < n; ++i) {
        auto name = counted([&](char* o, kal_uintptr cap) { return kal_env_var_at(i, o, cap); });
        if (!name) continue;
        auto value = get(*name);
        out.push_back(*name + "=" + value.value_or(""));
    }
    return out;
}

std::vector<std::pair<std::string, std::optional<std::string>>> assignments(std::span<const std::string> entries) {
    std::vector<std::pair<std::string, std::optional<std::string>>> out;
    out.reserve(entries.size());
    for (const auto& entry : entries) {
        const auto equals = entry.find('=', 1);
        if (equals == std::string::npos) continue;
        out.emplace_back(entry.substr(0, equals), entry.substr(equals + 1));
    }
    return out;
}

std::vector<std::string> arguments() {
    std::vector<std::string> out;
    const kal_uintptr n = kal_env_arg_count();
    for (kal_uintptr i = 0; i < n; ++i)
        out.push_back(counted([&](char* o, kal_uintptr cap) { return kal_env_arg(i, o, cap); }).value_or(""));
    return out;
}

std::optional<std::filesystem::path> image() {
    auto s = counted([](char* o, kal_uintptr cap) { return kal_env_image(o, cap); });
    if (!s || s->empty()) return std::nullopt;
    return fs::from_utf8(*s);
}

std::optional<std::filesystem::path> loader() {
    auto s = counted([](char* o, kal_uintptr cap) { return kal_env_loader(o, cap); });
    if (!s || s->empty()) return std::nullopt;
    return fs::from_utf8(*s);
}

std::vector<std::string> self_command() {
    std::vector<std::string> out;
    const auto program = loader();
    const auto own = image();
    // Absolute, so that the command starts the program from any directory: an
    // image is named as its loader was given it, which may be relative.
    if (program) out.push_back(fs::utf8(fs::absolute(*program)));
    if (own && (!program || *own != *program)) out.push_back(fs::utf8(fs::absolute(*own)));
    return out;
}

std::optional<std::string> system() {
    return counted([](char* o, kal_uintptr cap) { return kal_env_system(o, cap); });
}

std::vector<std::filesystem::path> split_list(std::string_view value) {
    std::vector<std::filesystem::path> out;
    const bool windows = host() == platform::windows;
    const char sep = conventions().path_list_separator;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(sep, start);
        auto piece = value.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        while (!piece.empty() && (piece.front() == ' ' || piece.front() == '\t')) piece.remove_prefix(1);
        while (!piece.empty() && (piece.back() == ' ' || piece.back() == '\t')) piece.remove_suffix(1);
        if (windows && piece.size() >= 2 && piece.front() == '"' && piece.back() == '"') piece = piece.substr(1, piece.size() - 2);
        if (!piece.empty()) out.emplace_back(std::string(piece));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return out;
}

std::string join_list(std::span<const std::filesystem::path> directories) {
    std::string out;
    const char sep = conventions().path_list_separator;
    for (const auto& d : directories) {
        if (!out.empty()) out += sep;
        out += fs::native(d);
    }
    return out;
}

std::vector<std::filesystem::path> search_path() { return split_list(get("PATH").value_or("")); }

namespace {
bool is_program(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec);
}
}  // namespace

std::optional<std::filesystem::path> find_executable(std::string_view name, std::string_view path_list) {
    if (name.empty()) return std::nullopt;
    const bool windows = host() == platform::windows;
    // On Windows what makes a file a program is its extension. A name that
    // carries one of PATHEXT's is looked for as written; any other has each of
    // them appended and is never looked for bare: an extension-less file
    // beside `npm.cmd' is the POSIX shell script `npm', which this system
    // cannot start.
    std::vector<std::string> suffixes{ "" };
    if (windows) {
        const auto ext = get("PATHEXT").value_or(".COM;.EXE;.BAT;.CMD");
        std::string lower_name(name);
        for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::vector<std::string> pieces;
        std::size_t s = 0;
        while (s <= ext.size()) {
            const auto e = ext.find(';', s);
            auto piece = ext.substr(s, e == std::string::npos ? std::string::npos : e - s);
            for (auto& c : piece) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!piece.empty()) pieces.push_back(piece);
            if (e == std::string::npos) break;
            s = e + 1;
        }
        const bool carries = std::ranges::any_of(pieces, [&](const std::string& p) { return lower_name.ends_with(p); });
        if (!carries) {
            suffixes = pieces;
            // A name the system starts directly comes before one it would hand
            // to an interpreter, as the system's own search orders them.
            std::ranges::stable_partition(suffixes, [](const std::string& x) { return x == ".exe" || x == ".com"; });
        }
    }
    const std::filesystem::path given(name);
    if (fs::names_a_place(name)) {
        for (const auto& s : suffixes) {
            auto candidate = given; candidate += s;
            if (is_program(candidate)) return candidate;
        }
        return std::nullopt;
    }
    for (const auto& dir : split_list(path_list))
        for (const auto& s : suffixes) {
            auto candidate = dir / given; candidate += s;
            if (is_program(candidate)) return candidate;
        }
    return std::nullopt;
}

std::optional<std::filesystem::path> find_executable(std::string_view name) {
    return find_executable(name, get("PATH").value_or(""));
}

}  // namespace mcpplibs::os::env
