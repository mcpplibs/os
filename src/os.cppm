// mcpplibs.os --- every module of the library.
//
// The platform modules are re-exported too, so that they are public modules of
// the package (mcpp SPEC-008: the root and what it re-exports): their names are
// in a namespace of their platform (`os::windows::elevated'), and a call outside
// `if constexpr' for that platform is a link error on every other (R5).
export module mcpplibs.os;

export import mcpplibs.os.platform;
export import mcpplibs.os.error;
export import mcpplibs.os.env;
export import mcpplibs.os.fs;
export import mcpplibs.os.process;
export import mcpplibs.os.interrupt;
export import mcpplibs.os.stdio;
export import mcpplibs.os.terminal;
export import mcpplibs.os.dirs;
export import mcpplibs.os.ipc;
export import mcpplibs.os.net;
export import mcpplibs.os.machine;
export import mcpplibs.os.random;
export import mcpplibs.os.library;
export import mcpplibs.os.linux;
export import mcpplibs.os.macos;
export import mcpplibs.os.windows;
export import mcpplibs.os.posix;
