// mcpplibs.os.windows in an ok image (0.4.0, R5 as revised): an image reaches
// no Win32 function (it loads no library of the system, openkal SPEC clause
// 12), so what it answers it asks a program of the system for. Only the facts
// a program of Windows reports are defined here; the code pages, long names,
// the registry and the drives a user writes to are okrun's affair or the
// native build's, and remain undefined in an image (a use is a link error).
//
// `host()' decides which system's question applies, which is a convention of
// that system and not a capability of openkal's implementation (SPEC clause
// 30); whether a program can be started at all is openkal's capability, and a
// refusal is answered as "not known".
module mcpplibs.os.windows;

import std;
import mcpplibs.os.platform;
import mcpplibs.os.env;
import mcpplibs.os.process;

namespace mcpplibs::os::windows {

namespace {

// `%SystemRoot%\System32\<tool>.exe', never a name looked for on PATH.
std::optional<std::filesystem::path> system_tool(std::string_view tool) {
    auto root = env::get("SystemRoot");
    if (!root || root->empty()) return std::nullopt;
    std::string name(tool);
    name += ".exe";
    return std::filesystem::path(*root) / "System32" / name;
}

// What the tool printed, when it ran to an end with status 0.
std::optional<std::string> printed(std::string_view tool, std::vector<std::string> args) {
    auto program = system_tool(tool);
    if (!program) return std::nullopt;
    process::spawn_options o;
    o.program = *program;
    o.args = std::move(args);
    o.in = process::stdio::null;
    o.err = process::stdio::null;
    process::bounds limits;
    limits.hard = std::chrono::seconds{10};
    auto r = process::run(std::move(o), limits);
    if (!r || r->timed_out || !r->status.success()) return std::nullopt;
    return std::move(r->out);
}

}  // namespace

// The token's integrity level as `whoami /groups' lists it: High (S-1-16-12288)
// or System (S-1-16-16384) is the administrator's token. A SID, not a group's
// name, so the answer does not depend on the system's language.
bool elevated() {
    if (host() != platform::windows) return false;
    static const bool answer = [] {
        const auto text = printed("whoami", {"/groups"});
        return text && (text->find("S-1-16-12288") != std::string::npos
                        || text->find("S-1-16-16384") != std::string::npos);
    }();
    return answer;
}

// `fsutil devdrv query <volume>' names a developer volume as one; its answer is
// read only when the tool states one of its two sentences, and is otherwise
// not known (a system in another language, or a user the tool refuses).
std::optional<bool> on_dev_drive(const std::filesystem::path& directory) {
    if (host() != platform::windows) return std::nullopt;
    const auto text = directory.generic_string();
    if (text.size() < 2 || text[1] != ':') return std::nullopt;
    const auto said = printed("fsutil", {"devdrv", "query", text.substr(0, 2)});
    if (!said) return std::nullopt;
    if (said->find("not a developer volume") != std::string::npos) return false;
    if (said->find("developer volume") != std::string::npos) return true;
    return std::nullopt;
}

}  // namespace mcpplibs::os::windows
