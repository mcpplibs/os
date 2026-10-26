// The architecture of this build: x86_64. Selected by the manifest's
// [target.'cfg(arch = "x86_64")'.build].
export module mcpplibs.os.platform:arch;

export namespace mcpplibs::os {

enum class architecture { x86_64, aarch64, riscv64 };

// What the program is built for. The machine it runs upon may differ (a
// translated program); `machine::arch()' answers that.
inline constexpr architecture ARCH = architecture::x86_64;

}  // namespace mcpplibs::os
