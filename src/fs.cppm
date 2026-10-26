// mcpplibs.os.fs --- what std::filesystem does not say.
//
// std::filesystem names, creates, enumerates and removes, and this module does
// not repeat it (R0). What is here is what it cannot express: a write that is
// all or nothing, a publication that does not replace, an exclusion that ends
// with its holder, what a volume can do, links on every system, whether a node
// may be started, and the identity of a node. Every operation reaches the
// kernel through openkal, upon the supplied directory an absolute name lies
// beneath (`locate').
export module mcpplibs.os.fs;

import std;
export import mcpplibs.os.error;

export namespace mcpplibs::os::fs {

// Where a name lies, in openkal's terms: a supplied directory (`kal_dir', a
// borrowed handle carried as its word) and the remainder beneath it, without a
// leading separator; "." for the directory itself. For a program that also
// calls openkal itself.
struct located {
    std::uintptr_t directory = 0;
    std::string    preopen;
    std::string    relative;
};

// The supplied directory a name lies beneath: the longest whose name is a
// prefix of the name made absolute against the working directory, compared
// without case where the host's names are. Nothing, for a name beneath none.
std::optional<located> locate(const std::filesystem::path& name);

// The supplied directories' names, in openkal's order.
std::vector<std::string> preopens();

// A name made absolute in the host's spelling: one absolute already is kept (on
// Windows a drive and a separator, or two separators; elsewhere a leading `/'),
// and another is joined to the working directory. Asked of `host()' and not of
// the standard library, whose answer in an ok image is POSIX's whatever system
// the image runs upon.
std::filesystem::path absolute(const std::filesystem::path& name);

// A name's text in UTF-8 and back, never through the process's code page.
// UTF-8 is what openkal's names are on every system; `path::string()` and
// `path(std::string)` use the ANSI code page on Windows, where a name it
// cannot spell throws and one it can is not UTF-8.
std::string utf8(const std::filesystem::path& name);
std::string generic_utf8(const std::filesystem::path& name);  // with '/' separators
std::filesystem::path from_utf8(std::string_view text);

// NAMES IN THE HOST'S RULES, AND WITHOUT ASKING THE FILE SYSTEM. Each is lexical:
// it reads the name and never the node, so a link is not followed and a name
// that does not exist is answered as well as one that does (`identity_of' says
// whether two names reach one node). All ask `host()'.
//
// `generic': the spelling every interface here reads --- `/' between
// components, `\' read as one on Windows, `.' and `..' resolved, no trailing
// separator; the root kept as written (`/', `C:/', `//server/share/').
// `native': the same in the host's own separator, for a program's argument
// vector or a person (`C:\Users\x').
// `is_absolute': begins with a root (on Windows a drive and a separator, or
// `//server/share'; `C:x' is relative to the drive's current directory).
// `same': one spelling, compared without case where the host's names are.
// `within': the name is the directory or beneath it, by components.
std::string generic(const std::filesystem::path& name);
std::string native(const std::filesystem::path& name);
bool is_absolute(const std::filesystem::path& name);
bool same(const std::filesystem::path& a, const std::filesystem::path& b);
bool within(const std::filesystem::path& name, const std::filesystem::path& directory);

// Whether a name says where a file is, rather than naming one to look for on a
// list of directories: it holds a separator in the host's spelling (on Windows
// `\' as well as `/', or a drive). Asked of `host()' for the reason above.
bool names_a_place(std::string_view name);

// --- contents --------------------------------------------------------------

result<std::string> read(const std::filesystem::path& file);

// Written in place: created, or truncated and rewritten.
result<void> write(const std::filesystem::path& file, std::string_view bytes);

// ALL OR NOTHING. Written beside the name, made durable, then renamed over it:
// a reader sees the old content or the new and never part of either.
result<void> write_atomic(const std::filesystem::path& file, std::string_view bytes);

// WRITTEN UNDER A NAME ONLY IF NOBODY HAS: fails with errc::exists, changing
// nothing, when the name is there. On a volume that renames without replacing
// (openkal's KAL_FS_PROP_RENAME_NO_REPLACE) it is all or nothing as well; on
// one that cannot --- NFS version 3 among them --- the name is created
// exclusively and then written, so a reader may see it empty, and
// `published::atomic' says which happened.
struct published { bool atomic = false; };
result<published> publish(const std::filesystem::path& file, std::string_view bytes);

// Appended in one write that another program appending to the same file cannot
// split; the file is created when it is not there.
result<void> append(const std::filesystem::path& file, std::string_view bytes);

// What was written to the file, or to the directory's entries, is durable: it
// survives the machine stopping. A volume that keeps nothing durable (a
// directory on Windows, an image's preopened tree) has nothing to do.
result<void> sync(const std::filesystem::path& node);

// The name in the spelling that reaches past the system's limit on a name's
// length: `\\?\' (or `\\?\UNC\') before an absolute name on Windows, with `.'
// and `..' resolved because that spelling no longer resolves them; the name
// itself everywhere else, and for a name that is not absolute.
std::filesystem::path extended(const std::filesystem::path& name);
// The Windows rule of `extended', of an absolute name in either separator, so
// that it is a fact on every system.
std::string windows_extended(std::string_view absolute_name);

// --- what a volume can do ----------------------------------------------------

struct capabilities {
    bool case_sensitive    = false;
    bool links             = false;  // nodes whose content is another name are met
    bool make_links        = false;  // and are made
    bool rename_no_replace = false;  // `publish' is all or nothing
    bool local_endpoint    = false;  // an `ipc::local' name may be here
    bool locks             = false;
    bool executable        = false;  // whether a node may be started is recorded
    bool atomic_rename     = false;
    bool remove_open       = false;  // a name held open is removed at once
};

// Of the volume the directory is on.
result<capabilities> caps(const std::filesystem::path& directory);

// WHAT THE VOLUME DOES, MEASURED (0.2.0). A volume is whatever a person has
// --- ext4, APFS, NTFS, and NFSv3, SMB, FAT, a FUSE mount, WSL's drvfs --- and
// what it claims is not always what it does: NFSv3 answers a rename that may
// not replace with EINVAL. Measured once per volume per program, with a
// scratch directory in `directory' that is removed after; `measured' false
// when the directory cannot be written, and the system's own answers stand.
struct behaviour {
    std::string type;                 // "ext4", "nfs", "apfs", "NTFS", ...; "" unknown
    bool rename_no_replace = true;    // a rename that refuses an existing name, atomically
    bool exchange          = true;    // two entries swapped atomically
    bool hard_links        = true;
    bool symbolic_links    = true;
    bool case_sensitive    = true;
    bool measured          = false;
    // Whether every operation below is the volume's own.
    bool native() const { return rename_no_replace && exchange && hard_links && symbolic_links; }
};
behaviour measure(const std::filesystem::path& directory);
// The same, measured now and not remembered.
behaviour measure_now(const std::filesystem::path& directory);
// The volume's kind, as the system names it; "" where it does not.
std::string volume_type(const std::filesystem::path& directory);

// Who owns a node: this program's user, the system (the superuser; on Windows
// SYSTEM or the Administrators), or another user.
enum class owner { this_user, system, another_user, unknown };
owner owner_of(const std::filesystem::path& node);
// Whether this program's user may write the node (POSIX `access(W_OK)';
// elsewhere what the system's own rules decide when the write is made, so
// true).
bool writable(const std::filesystem::path& node);

// An entry made beside `to' (on its volume) published under `to', never over an
// entry there: by a rename that refuses replacement where the volume has one;
// else by a hard link (a file) or the same symbolic link again, each refused
// as atomically when the name exists, the scratch name then removed; else, for
// a directory, by a rename onto a name that is not there --- the one window, a
// name made in between, is the caller's under its own exclusion.
result<void> place(const std::filesystem::path& from, const std::filesystem::path& to);

// `a' and `b' (both present, on one volume) trade places: atomically where the
// volume exchanges; else `b' is moved aside, `a' renamed onto `b''s name and the
// old `b' put at `a' --- a reader of `b' meets the name missing for that moment,
// never a half of either.
result<void> exchange(const std::filesystem::path& a, const std::filesystem::path& b);

// --- exclusion -----------------------------------------------------------------

// A lock upon a file, held until released or until the holder ends however it
// ends. The file is created if it is not there.
class lock {
public:
    enum class kind { shared, exclusive };
    static result<lock> acquire(const std::filesystem::path& file, kind k, bool wait);

    lock() = default;
    lock(lock&&) noexcept;
    lock& operator=(lock&&) noexcept;
    lock(const lock&) = delete;
    lock& operator=(const lock&) = delete;
    ~lock();

    bool held() const noexcept { return file_ != 0; }
    void release() noexcept;

private:
    std::uintptr_t file_ = 0;
};

// --- links, programs, identity ------------------------------------------------

// A node at `link' whose content is `target'. `directory' says the target is a
// directory, which one system must be told; there a directory's link that a
// symbolic link cannot be is a junction, whose content is the absolute name.
result<void> create_link(const std::filesystem::path& target, const std::filesystem::path& link,
                         bool directory);
result<std::string> read_link(const std::filesystem::path& link);

// Whether the node may be started, on a volume that records it.
result<void> set_executable(const std::filesystem::path& file, bool executable);

struct stamp {
    std::uint64_t size = 0;
    std::uint64_t modified_ns = 0;
    auto operator<=>(const stamp&) const = default;
};
result<stamp> stamp_of(const std::filesystem::path& node);

// Two names are one node exactly when their identities are equal.
struct identity {
    std::uint64_t volume = 0;
    std::uint64_t index = 0;
    auto operator<=>(const identity&) const = default;
};
result<identity> identity_of(const std::filesystem::path& node);

// A rename; `replace' false fails with errc::exists when `to' is there.
result<void> rename(const std::filesystem::path& from, const std::filesystem::path& to,
                    bool replace = true);

}  // namespace mcpplibs::os::fs

// Each system's own (src/<os>/fs.cpp).
namespace mcpplibs::os::fs::sys {
result<void> sync(const std::filesystem::path& node);
// 0, or the system's condition; `exchange_unsupported' says the volume declined.
int exchange(const std::filesystem::path& a, const std::filesystem::path& b);
bool exchange_unsupported(int condition);
std::string volume_type(const std::filesystem::path& directory);
owner owner_of(const std::filesystem::path& node);
bool writable(const std::filesystem::path& node);
}  // namespace mcpplibs::os::fs::sys
