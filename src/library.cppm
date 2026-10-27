// mcpplibs.os.library --- shared libraries the program loads at run time, and
// the symbols the program and its libraries define.
//
//   auto lib = os::library::open("plugins/search.so");   // or a name the system searches for
//   auto f = lib->function<int(int)>("square");          // nothing when it defines none
//
// What a plugin system needs (a module a program loads by name), and what an
// extension loaded by another program needs: the functions of the program
// that loaded it (`process_symbol', `loaded'), which it did not link against.
// A library stays loaded while a handle to it is open; `loaded' does not load
// one and does not keep it.
export module mcpplibs.os.library;

import std;
import mcpplibs.os.error;

export namespace mcpplibs::os::library {

class handle {
public:
    handle() noexcept = default;
    handle(handle&& other) noexcept : native_(std::exchange(other.native_, nullptr)), owned_(std::exchange(other.owned_, false)) {}
    handle& operator=(handle&& other) noexcept;
    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;
    ~handle();

    // The address of a symbol the library defines; nothing when it defines none.
    [[nodiscard]] void* symbol(std::string_view name) const;
    template <class F>
    [[nodiscard]] F* function(std::string_view name) const {
        return reinterpret_cast<F*>(symbol(name));
    }
    [[nodiscard]] bool valid() const noexcept { return native_ != nullptr; }

    // For the implementations.
    handle(void* native, bool owned) noexcept : native_(native), owned_(owned) {}

private:
    void* native_ = nullptr;
    bool  owned_ = false;  // opened here: closed with the handle
};

// Loads a library: a path, or a bare name (`libz.so.1', `zlib1.dll') the
// system searches for as it does for a program's own. Its symbols are its
// own: they do not join the program's (RTLD_LOCAL).
result<handle> open(const std::filesystem::path& file);

// A library already in the process, by the name it was loaded under
// (`python3.dll', `libpython3.12.so.1.0'); nothing when it is not loaded.
std::optional<handle> loaded(std::string_view name);

// A symbol of the program as the loader resolves one: the executable, then
// every library loaded into the process's global scope (on Windows, every
// module of the process, in load order). Nothing when none defines it.
[[nodiscard]] void* process_symbol(std::string_view name);

// The suffix of a shared library of this system: `.so', `.dylib', `.dll'.
[[nodiscard]] std::string_view suffix() noexcept;

}  // namespace mcpplibs::os::library
