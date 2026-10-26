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
};

// Of the volume the directory is on.
result<capabilities> caps(const std::filesystem::path& directory);

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
