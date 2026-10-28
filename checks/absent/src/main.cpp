// A program that names itself: openkal.process, which Emscripten does not have.
import std;
import mcpplibs.os;

int main() { return mcpplibs::os::process::self().has_value() ? 0 : 1; }
