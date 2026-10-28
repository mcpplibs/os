// The architecture of this build: wasm32. Selected by the manifest's
// [target.'cfg(arch = "wasm32")'.build].
export module mcpplibs.os.platform:arch;

export namespace mcpplibs::os {

enum class architecture { x86_64, aarch64, riscv64, wasm32 };

// What the program is built for. A wasm module runs upon whatever machine its
// host runs upon, and is told nothing of it; `machine::arch()' answers wasm32.
inline constexpr architecture ARCH = architecture::wasm32;

}  // namespace mcpplibs::os
