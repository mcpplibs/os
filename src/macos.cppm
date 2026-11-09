// mcpplibs.os.macos --- what macOS has and the others do not (R5).
export module mcpplibs.os.macos;

import std;
import mcpplibs.os.error;

export namespace mcpplibs::os::macos {

// Whether this program is translated (Rosetta). A program built for one
// architecture and run on another has no portable way to ask, and the answer
// matters only here.
bool translated();

}  // namespace mcpplibs::os::macos

// THE OBJECTIVE-C RUNTIME (0.4.4): how the system's frameworks are called
// (Foundation, AppKit, UserNotifications). No other system has it (R2).
// libobjc and the frameworks are loaded when first used (`os::library'), so a
// program links nothing for them; an image loads no library of the system,
// and these are not defined there (R5): a program that is to run as an image
// asks `if constexpr (os::is_macos)', the question of a native build.
//
//   objc::autorelease_pool pool;
//   void* info = objc::send<void*>(objc::class_named("NSProcessInfo"), "processInfo");
//   std::string name = objc::utf8(objc::send<void*>(info, "processName"));
//
// A message is objc_msgSend typed as the method is: `send<R>(receiver,
// selector, args...)'. A result returned in memory rather than in registers
// (a structure larger than two words on x86-64) is not sent this way.
export namespace mcpplibs::os::macos::objc {

// One reference to an object, released with this and retained by a copy
// (objc_retain and objc_release are paired here, R6).
class id {
public:
    id() noexcept = default;
    id(const id& other) noexcept;
    id(id&& other) noexcept : raw_(std::exchange(other.raw_, nullptr)) {}
    id& operator=(id other) noexcept {
        std::swap(raw_, other.raw_);
        return *this;
    }
    ~id();

    // A reference the caller owns (what alloc, new, copy and init return) becomes this one.
    static id adopt(void* raw) noexcept;
    // A reference of its own to an object another holds (an autoreleased result).
    static id borrow(void* raw) noexcept;

    [[nodiscard]] void* raw() const noexcept { return raw_; }
    explicit operator bool() const noexcept { return raw_ != nullptr; }

private:
    void* raw_ = nullptr;
};

// Whether the runtime is here, with Foundation (NSString, NSArray, ...):
// not_supported where this program cannot load a library (a static program).
result<void> available();
// A framework of the system, by name ("UserNotifications"): its classes exist after.
result<void> load_framework(std::string_view name);
// A class by name (null: none), a selector by name, objc_msgSend (null: no runtime).
[[nodiscard]] void* class_named(std::string_view name);
[[nodiscard]] void* selector(std::string_view name);
[[nodiscard]] void* message_function();

// A message to `receiver'; with no runtime, nothing is sent and R's empty value returned.
template <class R = void, class... A>
R send(void* receiver, std::string_view name, A... args) {
    using function = R (*)(void*, void*, A...);
    void* const sender = message_function();
    if (!sender) {
        if constexpr (std::is_void_v<R>) return;
        else return R{};
    }
    return reinterpret_cast<function>(sender)(receiver, selector(name), args...);
}
template <class R = void, class... A>
R send(const id& receiver, std::string_view name, A... args) {
    return send<R>(receiver.raw(), name, args...);
}

// The objects autoreleased while this lives are released with it.
class autorelease_pool {
public:
    autorelease_pool();
    autorelease_pool(const autorelease_pool&) = delete;
    autorelease_pool& operator=(const autorelease_pool&) = delete;
    ~autorelease_pool();

private:
    void* token_ = nullptr;
};

// An NSString from UTF-8, and the UTF-8 of one (R7).
[[nodiscard]] id string(std::string_view utf8);
[[nodiscard]] std::string utf8(void* string);
[[nodiscard]] inline std::string utf8(const id& string) { return utf8(string.raw()); }

// A class made here (a delegate): NSObject's or another's subclass with these
// methods. A method is its selector, a function `R(void* self, void* selector,
// A...)' and its type encoding ("v@:q"). A protocol the runtime does not know
// is passed over (a delegate is asked by its methods). A name already taken:
// exists.
struct method {
    std::string_view selector;
    void*            implementation;
    std::string_view types;
};
result<void*> define_class(std::string_view name, std::string_view superclass, std::span<const method> methods,
                           std::span<const std::string_view> protocols = {});

namespace detail {
// What a block holds: the function, counted across the runtime's copies of the block.
struct block_body {
    std::atomic<std::uint32_t> references{1};
    void (*destroy)(block_body*) = nullptr;
};
// The block's layout (the Blocks ABI): a literal, then what it captures.
struct block_literal {
    void*         isa;
    std::int32_t  flags;
    std::int32_t  reserved;
    void*         invoke;
    const void*   descriptor;
    block_body*   body;
};
id make_block(void* invoke, block_body* body);
}  // namespace detail

// A block (a completion handler, an enumeration's body) that runs `body': a
// copy the runtime holds, which the receiver may keep; `body' goes with the
// last reference to it.
template <class R, class... A>
id block(std::function<R(A...)> body) {
    struct typed : detail::block_body {
        std::function<R(A...)> function;
    };
    auto* held = new typed{};
    held->function = std::move(body);
    held->destroy = [](detail::block_body* self) { delete static_cast<typed*>(self); };
    R (*invoke)(detail::block_literal*, A...) = [](detail::block_literal* self, A... args) -> R {
        return static_cast<typed*>(self->body)->function(args...);
    };
    return detail::make_block(reinterpret_cast<void*>(invoke), held);
}

}  // namespace mcpplibs::os::macos::objc
