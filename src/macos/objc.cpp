// The Objective-C runtime (mcpplibs.os.macos, 0.4.4): libobjc's functions and
// libSystem's blocks, found when first used through this package's own
// `library', so that a program links nothing for them and the unit includes
// no header of the SDK.
module mcpplibs.os.macos;

import std;
import mcpplibs.os.error;
import mcpplibs.os.library;

namespace mcpplibs::os::macos::objc {

namespace {

// The runtime's functions, each found once; all null where nothing loads.
struct runtime_functions {
    library::handle objc, system, foundation;
    void* (*get_class)(const char*) = nullptr;
    void* (*register_selector)(const char*) = nullptr;
    void* message = nullptr;
    void* (*retain)(void*) = nullptr;
    void (*release)(void*) = nullptr;
    void* (*pool_push)() = nullptr;
    void (*pool_pop)(void*) = nullptr;
    void* (*allocate_class)(void*, const char*, std::size_t) = nullptr;
    void (*register_class)(void*) = nullptr;
    bool (*add_method)(void*, void*, void*, const char*) = nullptr;
    void* (*get_protocol)(const char*) = nullptr;
    bool (*add_protocol)(void*, void*) = nullptr;
    void* stack_block = nullptr;  // _NSConcreteStackBlock: the class of a block a program lays out itself
    void* (*block_copy)(const void*) = nullptr;
    std::optional<error> missing;

    runtime_functions() {
        if (!library::supported()) {
            missing = error{errc::not_supported, "the Objective-C runtime: this program loads no library (a static program)", 0};
            return;
        }
        auto opened = library::open("/usr/lib/libobjc.A.dylib");
        auto with_blocks = library::open("/usr/lib/libSystem.B.dylib");
        if (!opened || !with_blocks) {
            missing = error{errc::not_found, "the Objective-C runtime: libobjc or libSystem did not load", 0};
            return;
        }
        // Foundation's classes are the runtime's base (NSString, NSArray, NSProcessInfo): loaded with it.
        auto base = library::open("/System/Library/Frameworks/Foundation.framework/Foundation");
        if (!base) {
            missing = error{errc::not_found, "the Objective-C runtime: Foundation did not load", 0};
            return;
        }
        objc = std::move(*opened);
        system = std::move(*with_blocks);
        foundation = std::move(*base);
        get_class = objc.function<void*(const char*)>("objc_getClass");
        register_selector = objc.function<void*(const char*)>("sel_registerName");
        message = objc.symbol("objc_msgSend");
        retain = objc.function<void*(void*)>("objc_retain");
        release = objc.function<void(void*)>("objc_release");
        pool_push = objc.function<void*()>("objc_autoreleasePoolPush");
        pool_pop = objc.function<void(void*)>("objc_autoreleasePoolPop");
        allocate_class = objc.function<void*(void*, const char*, std::size_t)>("objc_allocateClassPair");
        register_class = objc.function<void(void*)>("objc_registerClassPair");
        add_method = objc.function<bool(void*, void*, void*, const char*)>("class_addMethod");
        get_protocol = objc.function<void*(const char*)>("objc_getProtocol");
        add_protocol = objc.function<bool(void*, void*)>("class_addProtocol");
        stack_block = system.symbol("_NSConcreteStackBlock");
        block_copy = system.function<void*(const void*)>("_Block_copy");
        if (!get_class || !register_selector || !message || !retain || !release || !pool_push || !pool_pop || !allocate_class ||
            !register_class || !add_method || !get_protocol || !add_protocol || !stack_block || !block_copy) {
            missing = error{errc::not_found, "the Objective-C runtime: a function it should have is absent", 0};
            message = nullptr;  // nothing is sent
        }
    }
};

const runtime_functions& runtime() {
    static const runtime_functions found;
    return found;
}

// The Blocks ABI's descriptor of a block with copy and dispose helpers.
struct block_descriptor {
    unsigned long reserved, size;
    void (*copy)(void*, const void*);
    void (*dispose)(const void*);
};

void let_go(detail::block_body* body) {
    if (body->references.fetch_sub(1, std::memory_order_acq_rel) == 1) body->destroy(body);
}

// The runtime copied the block's bytes: the copy holds the body too.
void copy_helper(void* destination, const void*) {
    static_cast<detail::block_literal*>(destination)->body->references.fetch_add(1, std::memory_order_relaxed);
}
void dispose_helper(const void* block) { let_go(static_cast<const detail::block_literal*>(block)->body); }

const block_descriptor descriptor{0, sizeof(detail::block_literal), &copy_helper, &dispose_helper};

constexpr std::int32_t has_copy_dispose = 1 << 25;  // BLOCK_HAS_COPY_DISPOSE

}  // namespace

id::id(const id& other) noexcept : raw_(other.raw_) {
    if (raw_ && runtime().retain) runtime().retain(raw_);
}

id::~id() {
    if (raw_ && runtime().release) runtime().release(raw_);
}

id id::adopt(void* raw) noexcept {
    id out;
    out.raw_ = raw;
    return out;
}

id id::borrow(void* raw) noexcept {
    if (raw && runtime().retain) runtime().retain(raw);
    return adopt(raw);
}

result<void> available() {
    if (runtime().missing) return std::unexpected(*runtime().missing);
    return {};
}

result<void> load_framework(std::string_view name) {
    if (auto here = available(); !here) return here;
    const std::string file = std::format("/System/Library/Frameworks/{0}.framework/{0}", name);
    auto opened = library::open(file);
    if (!opened) return std::unexpected(error{errc::not_found, std::format("the framework {}", name), opened.error().native});
    // Kept loaded for the program's life: its classes are registered with the runtime.
    static std::mutex guard;
    static std::vector<library::handle> loaded;
    const std::scoped_lock lock(guard);
    loaded.push_back(std::move(*opened));
    return {};
}

void* class_named(std::string_view name) {
    if (!runtime().get_class) return nullptr;
    return runtime().get_class(std::string(name).c_str());
}

void* selector(std::string_view name) {
    if (!runtime().register_selector) return nullptr;
    return runtime().register_selector(std::string(name).c_str());
}

void* message_function() { return runtime().message; }

autorelease_pool::autorelease_pool() : token_(runtime().pool_push ? runtime().pool_push() : nullptr) {}

autorelease_pool::~autorelease_pool() {
    if (token_ && runtime().pool_pop) runtime().pool_pop(token_);
}

id string(std::string_view utf8) {
    void* allocated = send<void*>(class_named("NSString"), "alloc");
    // initWithBytes:length:encoding: (NSUTF8StringEncoding = 4): a reference this owns.
    return id::adopt(send<void*>(allocated, "initWithBytes:length:encoding:", static_cast<const void*>(utf8.data()),
                                 static_cast<unsigned long>(utf8.size()), 4UL));
}

std::string utf8(void* string) {
    if (!string) return {};
    const autorelease_pool pool;  // UTF8String's buffer may be autoreleased
    const auto length = send<unsigned long>(string, "lengthOfBytesUsingEncoding:", 4UL);
    const char* text = send<const char*>(string, "UTF8String");
    return text ? std::string(text, length) : std::string();
}

result<void*> define_class(std::string_view name, std::string_view superclass, std::span<const method> methods,
                           std::span<const std::string_view> protocols) {
    if (auto here = available(); !here) return std::unexpected(here.error());
    void* parent = class_named(superclass);
    if (!parent) return std::unexpected(error{errc::not_found, std::format("the class {}", superclass), 0});
    if (class_named(name)) return std::unexpected(error{errc::exists, std::format("the class {}", name), 0});
    void* made = runtime().allocate_class(parent, std::string(name).c_str(), 0);
    if (!made) return std::unexpected(error{errc::exists, std::format("the class {}", name), 0});
    for (const method& each : methods)
        if (!runtime().add_method(made, selector(each.selector), each.implementation, std::string(each.types).c_str()))
            return std::unexpected(error{errc::invalid, std::format("the method {} of {}", each.selector, name), 0});
    for (const std::string_view protocol : protocols)
        if (void* known = runtime().get_protocol(std::string(protocol).c_str())) (void)runtime().add_protocol(made, known);
    runtime().register_class(made);
    return made;
}

id detail::make_block(void* invoke, block_body* body) {
    if (!runtime().block_copy) {
        let_go(body);
        return {};
    }
    // Laid out here as a block on the stack is, then copied by the runtime: the
    // copy is the runtime's (its memory, its count), and this one goes.
    const block_literal laid{runtime().stack_block, has_copy_dispose, 0, invoke, &descriptor, body};
    void* copy = runtime().block_copy(&laid);
    let_go(body);  // the copy took its own (copy_helper)
    return id::adopt(copy);
}

}  // namespace mcpplibs::os::macos::objc
