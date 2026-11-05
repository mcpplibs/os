import std;
import mcpplibs.os.process;

int main() {
    auto observer = mcpplibs::os::process::unit_observer::open("opaque reference");
    mcpplibs::os::process::spawn_options options;
    options.declared_unit = &*observer; // An observer cannot admit a program.
    observer->terminate();             // It has no unit control operation.
}
