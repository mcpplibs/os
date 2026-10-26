// R5: a platform module used where it is not defined is a LINK error. This
// program calls a function of every platform module unconditionally; on each
// system the others are undefined, and the link must fail naming them.
import std;
import mcpplibs.os.linux;
import mcpplibs.os.macos;
import mcpplibs.os.windows;

int main() {
    return static_cast<int>(mcpplibs::os::linux::translator().has_value())
         + static_cast<int>(mcpplibs::os::macos::translated())
         + static_cast<int>(mcpplibs::os::windows::elevated());
}
