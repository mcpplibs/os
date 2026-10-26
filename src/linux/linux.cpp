module mcpplibs.os.linux;

import std;
import mcpplibs.os.env;

namespace mcpplibs::os::linux {

namespace {
std::string read_small(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return s;
}
}  // namespace

// The tracer's number in a status text: its `TracerPid:' line, or 0.
long tracer_of(std::string_view status) {
    const auto at = status.find("TracerPid:");
    if (at == std::string_view::npos) return 0;
    std::size_t i = at + 10;
    while (i < status.size() && (status[i] == ' ' || status[i] == '\t')) ++i;
    long v = 0;
    const auto [end, ec] = std::from_chars(status.data() + i, status.data() + status.size(), v);
    if (ec != std::errc{} || (end != status.data() + status.size() && *end != '\n' && *end != '\r')) return 0;
    return v > 0 ? v : 0;
}

std::optional<std::string> classify_translator(const translator_facts& facts) {
    std::string_view tracer = facts.tracer_name;
    while (!tracer.empty() && std::isspace(static_cast<unsigned char>(tracer.back()))) tracer.remove_suffix(1);
    while (!tracer.empty() && std::isspace(static_cast<unsigned char>(tracer.front()))) tracer.remove_prefix(1);
    if (tracer_of(facts.status) != 0) {
        if (tracer.starts_with("proot")) return std::string("proot");
        if (!tracer.empty()) return std::nullopt;       // another tracer, named: a debugger
    } else if (facts.status.find("TracerPid:") != std::string::npos) {
        return std::nullopt;                            // traced by nothing: a leaked PROOT_ means nothing
    }
    // The status or the tracer's name could not be read: the variables are what is left.
    for (const auto& v : facts.environment)
        if (v.starts_with("PROOT_")) return std::string("proot");
    return std::nullopt;
}

std::optional<std::string> translator() {
    translator_facts facts;
    facts.status = read_small("/proc/self/status");
    if (const long tracer = tracer_of(facts.status); tracer != 0)
        facts.tracer_name = read_small(std::format("/proc/{}/comm", tracer).c_str());
    facts.environment = env::variables();
    return classify_translator(facts);
}

}  // namespace mcpplibs::os::linux
