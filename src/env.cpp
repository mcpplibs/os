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
    return std::filesystem::path(*s);
}

std::optional<std::string> system() {
    return counted([](char* o, kal_uintptr cap) { return kal_env_system(o, cap); });
}

std::vector<std::filesystem::path> search_path() {
    std::vector<std::filesystem::path> out;
    const auto value = get("PATH");
    if (!value) return out;
    const char sep = conventions().path_list_separator;
    std::size_t start = 0;
    while (start <= value->size()) {
        const auto end = value->find(sep, start);
        const auto piece = value->substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!piece.empty()) out.emplace_back(piece);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

namespace {
bool is_program(const std::filesystem::path& p) {
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec);
}
}  // namespace

std::optional<std::filesystem::path> find_executable(std::string_view name, std::string_view path_list) {
    if (name.empty()) return std::nullopt;
    const bool windows = host() == platform::windows;
    std::vector<std::string> suffixes{ "" };
    if (windows) {
        const auto ext = get("PATHEXT").value_or(".COM;.EXE;.BAT;.CMD");
        std::string lower_name(name);
        for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::size_t s = 0;
        while (s <= ext.size()) {
            const auto e = ext.find(';', s);
            auto piece = ext.substr(s, e == std::string::npos ? std::string::npos : e - s);
            for (auto& c : piece) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!piece.empty() && !lower_name.ends_with(piece)) suffixes.push_back(piece);
            if (e == std::string::npos) break;
            s = e + 1;
        }
        // A name the system starts directly comes before one it would hand to
        // an interpreter, as the system's own search orders them.
        std::ranges::stable_partition(suffixes, [](const std::string& x) { return x.empty() || x == ".exe" || x == ".com"; });
    }
    const std::filesystem::path given(name);
    if (given.has_parent_path()) {
        for (const auto& s : suffixes) {
            auto candidate = given; candidate += s;
            if (is_program(candidate)) return candidate;
        }
        return std::nullopt;
    }
    const char sep = conventions().path_list_separator;
    std::size_t start = 0;
    while (start <= path_list.size()) {
        const auto end = path_list.find(sep, start);
        const auto dir = path_list.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (!dir.empty())
            for (const auto& s : suffixes) {
                auto candidate = std::filesystem::path(dir) / given; candidate += s;
                if (is_program(candidate)) return candidate;
            }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> find_executable(std::string_view name) {
    return find_executable(name, get("PATH").value_or(""));
}

}  // namespace mcpplibs::os::env
