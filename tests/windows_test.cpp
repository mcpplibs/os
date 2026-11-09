// mcpplibs.os.windows: COM and the Windows Runtime (0.4.4) --- an apartment,
// a Windows Runtime class's factory and an instance it makes, methods called
// by their slots, strings in four scripts there and back, an interface an
// object lacks refused, and a delegate of this program's own called back and
// let go. Elsewhere, and in an image, there is nothing to ask.
import std;
import mcpplibs.os;
import mcpplibs.mtest;

namespace os = mcpplibs::os;
using namespace mcpplibs::mtest;

UTest windows_tests{"windows", [] {
    if constexpr (os::is_windows) {
        namespace com = os::windows::com;

        "an identity read at compile time, laid out as the system's"_test = [] {
            constexpr auto unknown = com::id("{00000000-0000-0000-C000-000000000046}");
            static_assert(unknown == com::unknown);
            static_assert(com::id("94ea2b94-e9cc-49e0-c0ff-ee64ca8f5b90").data4[7] == 0x90);
            expect(eq(sizeof(com::guid), std::size_t{16}));
        };

        "a Windows Runtime class made, its methods called, its strings in four scripts there and back"_test = [] {
            auto apartment = com::apartment::enter(com::apartment::kind::multi_threaded);
            expect(fatal(apartment.has_value())) << (apartment ? std::string() : apartment.error().message());
            for (const std::string_view text : {"plain", "naïve façade", "Ελληνικά", "日本語のテキスト", "😀 emoji"}) {
                auto made = com::hstring::of(text);
                expect(made.has_value() && made->utf8() == text) << text;
            }
            // Windows.Foundation.Uri: IUriRuntimeClassFactory::CreateUri (slot 6), then
            // IUriRuntimeClass's Host (6 + 5), RawUri (6 + 10) and Port (6 + 13).
            constexpr auto uri_factory = com::id("44A9796F-723E-4FDF-A218-033E75B0C084");
            auto factory = com::factory("Windows.Foundation.Uri", uri_factory);
            expect(fatal(factory.has_value())) << (factory ? std::string() : factory.error().message());
            const std::string given = "https://example.org:8080/路径/ψ?q=1";
            auto text = com::hstring::of(given);
            expect(fatal(text.has_value()));
            void* made = nullptr;
            expect(fatal(factory->call(6, text->raw(), &made).has_value())) << "CreateUri";
            const auto uri = com::object::adopt(made);
            void* host = nullptr;
            expect(uri.call(11, &host).has_value());
            expect(com::hstring::adopt(host).utf8() == "example.org");
            void* raw = nullptr;
            expect(uri.call(16, &raw).has_value());
            expect(com::hstring::adopt(raw).utf8() == given) << "the text as given, through the system and back";
            std::int32_t port = 0;
            expect(uri.call(19, &port).has_value() && port == 8080);
            const auto as_inspectable = uri.query(com::inspectable);
            expect(as_inspectable.has_value()) << "every Windows Runtime object is IInspectable";
            const auto lacking = uri.query(com::id("11111111-2222-3333-4444-555555555555"));
            expect(!lacking && lacking.error().code == os::errc::not_supported) << "an interface it lacks: not_supported";
            const auto unknown_class = com::activate("Mcpplibs.Os.NoSuchClass");
            expect(!unknown_class && unknown_class.error().code == os::errc::not_found);
        };

        "a delegate of this program's own: asked for, called back with what it is given, let go"_test = [] {
            constexpr auto handler = com::id("A4C1F2B3-0D1E-4F5A-8B6C-7D8E9FA0B1C2");
            int called = 0;
            bool given_sender = false;
            const auto alive = std::make_shared<int>(0);
            std::weak_ptr<int> watch = alive;
            {
                auto made = com::delegate(handler, [&, held = alive](const com::object& sender, const com::object& args) {
                    ++called;
                    given_sender = static_cast<bool>(sender) && !args;
                });
                expect(fatal(made.has_value()));
                expect(made->query(com::unknown).has_value() && made->query(com::agile).has_value() && made->query(handler).has_value());
                expect(!made->query(com::inspectable).has_value()) << "only what it is";
                // The system's call: Invoke(sender, args), slot 3; the sender an object of the system's.
                auto apartment = com::apartment::enter(com::apartment::kind::multi_threaded);
                auto sender = com::factory("Windows.Foundation.Uri", com::id("44A9796F-723E-4FDF-A218-033E75B0C084"));
                expect(fatal(sender.has_value()));
                expect(made->call(3, sender->raw(), static_cast<void*>(nullptr)).has_value());
                expect(eq(called, 1) && given_sender);
            }
            expect(watch.use_count() == 1) << "let go with its last reference: only this test's own remains";
        };
    }
}};
