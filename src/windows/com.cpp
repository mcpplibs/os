// COM and the Windows Runtime (mcpplibs.os.windows, 0.5.0): ole32's and
// combase's functions, found when first used (LoadLibraryW and
// GetProcAddress, as advapi32's are in src/windows/windows.cpp), so that a
// program links nothing beyond kernel32 for them.
module;
#include "win32.h"

module mcpplibs.os.windows;

import std;
import mcpplibs.os.error;
import mcpplibs.os.fs;

#include "text.h"

namespace mcpplibs::os::windows::com {

namespace {

using hresult = std::int32_t;

constexpr hresult no_interface = static_cast<hresult>(0x80004002u);  // E_NOINTERFACE

template <class F>
F in(const char16_t* library, const char* name) {
    const okos_handle module = LoadLibraryW(library);
    return module ? reinterpret_cast<F>(GetProcAddress(module, name)) : nullptr;
}

// The functions, each found once; one a system lacks is null.
struct functions {
    hresult (__stdcall* initialize)(void*, okos_dword) = in<decltype(initialize)>(u"ole32.dll", "CoInitializeEx");
    void (__stdcall* uninitialize)() = in<decltype(uninitialize)>(u"ole32.dll", "CoUninitialize");
    hresult (__stdcall* create_instance)(const guid*, void*, okos_dword, const guid*, void**) =
        in<decltype(create_instance)>(u"ole32.dll", "CoCreateInstance");
    hresult (__stdcall* activate_instance)(void*, void**) = in<decltype(activate_instance)>(u"combase.dll", "RoActivateInstance");
    hresult (__stdcall* activation_factory)(void*, const guid*, void**) =
        in<decltype(activation_factory)>(u"combase.dll", "RoGetActivationFactory");
    hresult (__stdcall* create_string)(const char16_t*, std::uint32_t, void**) =
        in<decltype(create_string)>(u"combase.dll", "WindowsCreateString");
    hresult (__stdcall* delete_string)(void*) = in<decltype(delete_string)>(u"combase.dll", "WindowsDeleteString");
    const char16_t* (__stdcall* string_buffer)(void*, std::uint32_t*) =
        in<decltype(string_buffer)>(u"combase.dll", "WindowsGetStringRawBuffer");
};

const functions& system() {
    static const functions found;
    return found;
}

error absent(std::string_view what) { return error{errc::not_supported, std::format("{}: this system has no such function", what), 0}; }

// IUnknown's three, the first of every object's table.
void* const* table_of(void* raw) { return *static_cast<void* const* const*>(raw); }
hresult query_of(void* raw, const guid& interface, void** out) {
    return reinterpret_cast<hresult (*)(void*, const guid*, void**)>(table_of(raw)[0])(raw, &interface, out);
}
void add_ref(void* raw) { (void)reinterpret_cast<std::uint32_t (*)(void*)>(table_of(raw)[1])(raw); }
void release(void* raw) { (void)reinterpret_cast<std::uint32_t (*)(void*)>(table_of(raw)[2])(raw); }

// An object of this package's own that the system calls (a delegate). Its
// table is the first word, as the system reads every object's.
struct callback {
    void* const*                                        table;
    std::atomic<std::uint32_t>                          references{1};
    guid                                                interface;
    std::function<void(const object&, const object&)>   invoked;

    static hresult query(void* self, const guid* asked, void** out) {
        auto* held = static_cast<callback*>(self);
        if (*asked == unknown || *asked == agile || *asked == held->interface) {
            held->references.fetch_add(1, std::memory_order_relaxed);
            *out = self;
            return 0;
        }
        *out = nullptr;
        return no_interface;
    }
    static std::uint32_t add(void* self) { return static_cast<callback*>(self)->references.fetch_add(1, std::memory_order_relaxed) + 1; }
    static std::uint32_t drop(void* self) {
        auto* held = static_cast<callback*>(self);
        const std::uint32_t left = held->references.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (left == 0) delete held;
        return left;
    }
    static hresult invoke(void* self, void* sender, void* args) {
        static_cast<callback*>(self)->invoked(object::borrow(sender), object::borrow(args));
        return 0;
    }
};

void* const callback_table[] = {reinterpret_cast<void*>(&callback::query), reinterpret_cast<void*>(&callback::add),
                                reinterpret_cast<void*>(&callback::drop), reinterpret_cast<void*>(&callback::invoke)};

}  // namespace

result<void> checked(std::int32_t status, std::string_view context) {
    if (status >= 0) return {};
    const auto code = static_cast<std::uint32_t>(status);
    errc kind = errc::io;
    switch (code) {
        case 0x80004002u: case 0x80004001u: kind = errc::not_supported; break;  // E_NOINTERFACE, E_NOTIMPL
        case 0x80040154u: case 0x80070002u: case 0x80070490u: kind = errc::not_found; break;  // REGDB_E_CLASSNOTREG, not found
        case 0x80070005u: kind = errc::permission; break;  // E_ACCESSDENIED
        case 0x80070057u: case 0x80010106u: case 0x800401F0u: kind = errc::invalid; break;  // E_INVALIDARG, RPC_E_CHANGED_MODE, CO_E_NOTINITIALIZED
        case 0x8007000Eu: kind = errc::no_memory; break;  // E_OUTOFMEMORY
        default: break;
    }
    return std::unexpected(error{kind, std::format("{} (HRESULT 0x{:08X})", context, code), status});
}

result<apartment> apartment::enter(kind which) {
    if (!system().initialize) return std::unexpected(absent("CoInitializeEx"));
    // COINIT_APARTMENTTHREADED 0x2 or COINIT_MULTITHREADED 0x0, and COINIT_DISABLE_OLE1DDE 0x4.
    const okos_dword flags = (which == kind::single_threaded ? 0x2u : 0x0u) | 0x4u;
    if (auto entered = checked(system().initialize(nullptr, flags), "entering a COM apartment"); !entered)
        return std::unexpected(entered.error());
    return apartment(true);  // S_OK, or S_FALSE (entered again): either is left once
}

apartment& apartment::operator=(apartment&& other) noexcept {
    if (this != &other) {
        if (entered_) system().uninitialize();
        entered_ = std::exchange(other.entered_, false);
    }
    return *this;
}

apartment::~apartment() {
    if (entered_) system().uninitialize();
}

object::object(const object& other) noexcept : raw_(other.raw_) {
    if (raw_) add_ref(raw_);
}

object::~object() {
    if (raw_) release(raw_);
}

object object::adopt(void* raw) noexcept {
    object out;
    out.raw_ = raw;
    return out;
}

object object::borrow(void* raw) noexcept {
    if (raw) add_ref(raw);
    return adopt(raw);
}

result<object> object::query(const guid& interface) const {
    if (!raw_) return std::unexpected(error{errc::invalid, "an interface of no object", 0});
    void* out = nullptr;
    if (auto asked = checked(query_of(raw_, interface, &out), "asking an object for an interface"); !asked) return std::unexpected(asked.error());
    return adopt(out);
}

result<object> create(const guid& class_id, const guid& interface) {
    if (!system().create_instance) return std::unexpected(absent("CoCreateInstance"));
    void* out = nullptr;
    // CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER.
    if (auto made = checked(system().create_instance(&class_id, nullptr, 0x1u | 0x4u, &interface, &out), "creating a COM object"); !made)
        return std::unexpected(made.error());
    return object::adopt(out);
}

hstring& hstring::operator=(hstring&& other) noexcept {
    if (this != &other) {
        if (raw_ && system().delete_string) system().delete_string(raw_);
        raw_ = std::exchange(other.raw_, nullptr);
    }
    return *this;
}

hstring::~hstring() {
    if (raw_ && system().delete_string) system().delete_string(raw_);
}

result<hstring> hstring::of(std::string_view utf8) {
    if (!system().create_string) return std::unexpected(absent("WindowsCreateString"));
    const std::u16string wide = utf16_from_utf8(utf8);
    void* raw = nullptr;
    if (auto made = checked(system().create_string(wide.data(), static_cast<std::uint32_t>(wide.size()), &raw), "making a Windows Runtime string"); !made)
        return std::unexpected(made.error());
    return adopt(raw);
}

hstring hstring::adopt(void* raw) noexcept {
    hstring out;
    out.raw_ = raw;
    return out;
}

std::string hstring::utf8() const {
    if (!raw_ || !system().string_buffer) return {};
    std::uint32_t length = 0;
    const char16_t* text = system().string_buffer(raw_, &length);
    return text ? utf8_from_utf16(std::u16string_view(text, length)) : std::string();
}

result<object> activate(std::string_view class_name) {
    if (!system().activate_instance) return std::unexpected(absent("RoActivateInstance"));
    auto name = hstring::of(class_name);
    if (!name) return std::unexpected(name.error());
    void* out = nullptr;
    if (auto made = checked(system().activate_instance(name->raw(), &out), std::format("activating {}", class_name)); !made)
        return std::unexpected(made.error());
    return object::adopt(out);
}

result<object> factory(std::string_view class_name, const guid& interface) {
    if (!system().activation_factory) return std::unexpected(absent("RoGetActivationFactory"));
    auto name = hstring::of(class_name);
    if (!name) return std::unexpected(name.error());
    void* out = nullptr;
    if (auto found = checked(system().activation_factory(name->raw(), &interface, &out), std::format("the factory of {}", class_name)); !found)
        return std::unexpected(found.error());
    return object::adopt(out);
}

result<object> delegate(const guid& interface, std::function<void(const object& sender, const object& args)> invoked) {
    auto* made = new callback{callback_table, {}, interface, std::move(invoked)};
    made->references.store(1, std::memory_order_relaxed);
    return object::adopt(made);
}

}  // namespace mcpplibs::os::windows::com
