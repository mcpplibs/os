// mcpplibs.os.macos: the Objective-C runtime (0.4.4) --- strings in four
// scripts there and back, a framework's class asked and a message sent, a
// class defined here receiving messages, a block called back and let go. A
// program that loads no library (a static one) is told so. Elsewhere, and in
// an image, there is nothing to ask.
import std;
import mcpplibs.os;
import mcpplibs.mtest;

namespace os = mcpplibs::os;
using namespace mcpplibs::mtest;

UTest macos_tests{"macos", [] {
    if constexpr (os::is_macos) {
        namespace objc = os::macos::objc;

        "strings in four scripts there and back; a framework's class asked and a message sent"_test = [] {
            if (!os::library::supported()) {
                const auto here = objc::available();
                expect(!here && here.error().code == os::errc::not_supported) << "a static program: said, not pretended";
                return;
            }
            expect(fatal(objc::available().has_value()));
            const objc::autorelease_pool pool;
            for (const std::string_view text : {"plain", "naïve façade", "Ελληνικά", "日本語のテキスト", "😀 emoji"})
                expect(objc::utf8(objc::string(text)) == text) << text;
            expect(objc::load_framework("Foundation").has_value());
            const auto missing = objc::load_framework("McpplibsOsNoSuchFramework");
            expect(!missing && missing.error().code == os::errc::not_found);
            void* info = objc::send<void*>(objc::class_named("NSProcessInfo"), "processInfo");
            expect(info != nullptr);
            const std::string name = objc::utf8(objc::send<void*>(info, "processName"));
            expect(name.contains("macos_test")) << name;
            expect(objc::class_named("McpplibsOsNoSuchClass") == nullptr);
        };

        "a class defined here receives messages; a block is called back, then let go"_test = [] {
            if (!os::library::supported()) return;
            static long added = 0;
            void (*add)(void*, void*, long) = [](void*, void*, long amount) { added += amount; };
            const std::array methods{objc::method{"add:", reinterpret_cast<void*>(add), "v@:q"}};
            const auto made = objc::define_class("McpplibsOsCounter", "NSObject", methods);
            expect(fatal(made.has_value())) << (made ? std::string() : made.error().message());
            const auto again = objc::define_class("McpplibsOsCounter", "NSObject", methods);
            expect(!again && again.error().code == os::errc::exists);
            {
                const objc::autorelease_pool pool;
                const auto counter = objc::id::adopt(objc::send<void*>(objc::send<void*>(*made, "alloc"), "init"));
                objc::send(counter, "add:", 3L);
                objc::send(counter, "add:", 4L);
                expect(eq(added, 7L));
                expect(objc::send<bool>(counter, "respondsToSelector:", objc::selector("add:"))) << "the runtime knows the method";
            }
            const auto alive = std::make_shared<int>(0);
            const std::weak_ptr<int> watch = alive;
            int calls = 0;
            unsigned long indices = 0;
            {
                const objc::autorelease_pool pool;
                const std::array strings{objc::string("a"), objc::string("b"), objc::string("c")};
                const std::array<void*, 3> objects{strings[0].raw(), strings[1].raw(), strings[2].raw()};
                void* list = objc::send<void*>(objc::class_named("NSArray"), "arrayWithObjects:count:", objects.data(), 3UL);
                const auto body = objc::block(std::function<void(void*, unsigned long, signed char*)>(
                    [&, held = alive](void* object, unsigned long index, signed char*) {
                        ++calls;
                        indices += index;
                        expect(objc::utf8(object).size() == 1);
                    }));
                expect(fatal(static_cast<bool>(body)));
                objc::send(list, "enumerateObjectsUsingBlock:", body.raw());
            }
            expect(eq(calls, 3) && eq(indices, 3UL)) << "each element, in order";
            expect(watch.use_count() == 1) << "the block let go with its last reference: only this test's own remains";
        };
    }
}};
