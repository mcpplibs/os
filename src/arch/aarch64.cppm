// The architecture of this build: aarch64. Selected by the manifest's
// [target.'cfg(arch = "aarch64")'.build].
export module mcpplibs.os.platform:arch;

export namespace mcpplibs::os {

enum class architecture { x86_64, aarch64, riscv64 };

// What the program is built for. The machine it runs upon may differ (a
// translated program); `machine::arch()' answers that.
inline constexpr architecture ARCH = architecture::aarch64;

}  // namespace mcpplibs::os
