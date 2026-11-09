// mcpplibs.os.windows --- what Windows has and the others do not (R5).
export module mcpplibs.os.windows;

import std;
import mcpplibs.os.error;

export namespace mcpplibs::os::windows {

// Whether this program holds the administrator's token. Other systems have no
// single equivalent: their privileges are not a token a program holds.
bool elevated();

// Whether the directory is on a Dev Drive, which the system scans less and a
// build places its output on; nothing when the system cannot say.
std::optional<bool> on_dev_drive(const std::filesystem::path& directory);

// The ANSI code page the narrow interfaces of this process use (GetACP): 65001
// under an application manifest that asks for UTF-8 (0.2.0).
unsigned code_page();
// The output code page of the console the stream writes to; 0 when the stream
// is not a console.
unsigned console_output_code_page(bool error_stream = false);
// Whether the system lets a program that declares itself long-path aware use
// names past MAX_PATH (LongPathsEnabled).
bool long_paths_enabled();
// A directory named by a string value under HKEY_LOCAL_MACHINE, read in the
// 64-bit view and then the 32-bit one, without a trailing separator.
std::optional<std::filesystem::path> machine_registry_path(std::wstring_view key, std::wstring_view value);
// Whether `drive' is a fixed drive on which this user can create a file, and
// `child' under its root is absent or a plain directory.
bool writable_fixed_drive(char drive, std::wstring_view child);

}  // namespace mcpplibs::os::windows

// COM AND THE WINDOWS RUNTIME (0.4.4): the system's object model, through
// which the services that are not functions are reached (the shell's dialogs,
// toast notifications, the Windows Runtime's classes). No other system has it
// (R2). The libraries are found when first used (ole32, combase), so a program
// links nothing for it; an image loads no library of the system, and these are
// not defined there (R5): a program that is to run as an image asks
// `if constexpr (os::is_windows)', the question of a native build.
//
//   auto here = com::apartment::enter(com::apartment::kind::multi_threaded);
//   auto made = com::factory("Windows.Foundation.Uri", uri_factory);
//   void* uri = nullptr;
//   made->call(6, com::hstring::of("https://example.org").value().raw(), &uri);  // CreateUri
//   auto held = com::object::adopt(uri);
//
// A method is called by its slot in the object's table (IUnknown's are 0 to 2,
// IInspectable's 3 to 5), with the arguments its interface states; the result
// a method returns through a pointer is read from that pointer. The calling
// convention is the system's on every Windows target (x64, arm64).
export namespace mcpplibs::os::windows::com {

// An interface's or a class's identity, as the system lays it out.
struct guid {
    std::uint32_t                data1 = 0;
    std::uint16_t                data2 = 0, data3 = 0;
    std::array<std::uint8_t, 8>  data4{};
};
constexpr bool operator==(const guid& left, const guid& right) noexcept {
    return left.data1 == right.data1 && left.data2 == right.data2 && left.data3 == right.data3 && left.data4 == right.data4;
}

namespace detail {
// Not a constant expression: reached while parsing, it makes a malformed text a compile error.
inline void not_a_guid() {}
consteval std::uint32_t hex(std::string_view text) {
    std::uint32_t value = 0;
    for (const char c : text) {
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) not_a_guid();
        value = value * 16 + static_cast<std::uint32_t>(digit);
    }
    return value;
}
}  // namespace detail

// `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx', with or without braces, at compile time.
consteval guid id(std::string_view text) {
    if (text.size() == 38 && text.front() == '{' && text.back() == '}') text = text.substr(1, 36);
    if (text.size() != 36 || text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-') detail::not_a_guid();
    guid out;
    out.data1 = detail::hex(text.substr(0, 8));
    out.data2 = static_cast<std::uint16_t>(detail::hex(text.substr(9, 4)));
    out.data3 = static_cast<std::uint16_t>(detail::hex(text.substr(14, 4)));
    out.data4[0] = static_cast<std::uint8_t>(detail::hex(text.substr(19, 2)));
    out.data4[1] = static_cast<std::uint8_t>(detail::hex(text.substr(21, 2)));
    for (std::size_t index = 0; index < 6; ++index) out.data4[2 + index] = static_cast<std::uint8_t>(detail::hex(text.substr(24 + 2 * index, 2)));
    return out;
}

inline constexpr guid unknown = id("00000000-0000-0000-C000-000000000046");       // IUnknown
inline constexpr guid inspectable = id("AF86E2E0-B12D-4C6A-9C5A-D7AA65101E90");   // IInspectable
inline constexpr guid agile = id("94EA2B94-E9CC-49E0-C0FF-EE64CA8F5B90");         // IAgileObject

// What an HRESULT says: success, or the condition it names (E_NOINTERFACE is
// not_supported, REGDB_E_CLASSNOTREG not_found, E_ACCESSDENIED permission, ...),
// the HRESULT kept as the error's native value.
result<void> checked(std::int32_t status, std::string_view context);

// The calling thread in an apartment for as long as this lives
// (CoInitializeEx, then CoUninitialize). A thread already in an apartment of
// the same kind enters it again; of the other kind, `invalid'.
class apartment {
public:
    enum class kind : std::uint8_t { single_threaded, multi_threaded };
    static result<apartment> enter(kind which);

    apartment(apartment&& other) noexcept : entered_(std::exchange(other.entered_, false)) {}
    apartment& operator=(apartment&& other) noexcept;
    apartment(const apartment&) = delete;
    apartment& operator=(const apartment&) = delete;
    ~apartment();

private:
    explicit apartment(bool entered) noexcept : entered_(entered) {}
    bool entered_ = false;
};

// One reference to an object: released with this, taken again by a copy
// (AddRef and Release are paired here, R6).
class object {
public:
    object() noexcept = default;
    object(const object& other) noexcept;
    object(object&& other) noexcept : raw_(std::exchange(other.raw_, nullptr)) {}
    object& operator=(object other) noexcept {
        std::swap(raw_, other.raw_);
        return *this;
    }
    ~object();

    // The reference a call gave through its out parameter becomes this one.
    static object adopt(void* raw) noexcept;
    // A reference of its own to an object another holds (a parameter it was given).
    static object borrow(void* raw) noexcept;

    [[nodiscard]] void* raw() const noexcept { return raw_; }
    explicit operator bool() const noexcept { return raw_ != nullptr; }

    // The same object as another of its interfaces (QueryInterface).
    [[nodiscard]] result<object> query(const guid& interface) const;

    // The method at `slot' of its table, called with `args'.
    template <class... A>
    result<void> call(std::size_t slot, A... args) const {
        using method = std::int32_t (*)(void*, A...);
        if (!raw_) return std::unexpected(error{errc::invalid, "a COM method of no object", 0});
        void* const* table = *static_cast<void* const* const*>(raw_);
        return checked(reinterpret_cast<method>(table[slot])(raw_, args...), "a COM method");
    }

private:
    void* raw_ = nullptr;
};

// An instance of a registered class, as one of its interfaces (CoCreateInstance).
result<object> create(const guid& class_id, const guid& interface);

// The Windows Runtime's string (HSTRING), made from UTF-8 and read back as
// UTF-8 (R7).
class hstring {
public:
    hstring() noexcept = default;
    hstring(hstring&& other) noexcept : raw_(std::exchange(other.raw_, nullptr)) {}
    hstring& operator=(hstring&& other) noexcept;
    hstring(const hstring&) = delete;
    hstring& operator=(const hstring&) = delete;
    ~hstring();

    static result<hstring> of(std::string_view utf8);
    // The string a method gave through its out parameter becomes this one.
    static hstring adopt(void* raw) noexcept;
    [[nodiscard]] std::string utf8() const;
    [[nodiscard]] void* raw() const noexcept { return raw_; }

private:
    void* raw_ = nullptr;  // null: the empty string, as the system has it
};

// A Windows Runtime class's instance by its name, made with no arguments
// (RoActivateInstance), as IInspectable.
result<object> activate(std::string_view class_name);
// A Windows Runtime class's factory or its statics, as `interface'
// (RoGetActivationFactory).
result<object> factory(std::string_view class_name, const guid& interface);

// An object the system calls back: IUnknown, IAgileObject and `interface',
// whose one method beyond IUnknown's is Invoke(sender, args) --- the shape of
// every Windows Runtime event handler and completion. `invoked' runs on the
// thread the system calls on; the two are borrowed for the call.
result<object> delegate(const guid& interface, std::function<void(const object& sender, const object& args)> invoked);

}  // namespace mcpplibs::os::windows::com
