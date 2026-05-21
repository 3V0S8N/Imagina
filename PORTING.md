# Linux Port — Progress

## Phase 1 (done): Build chain green

`cmake -DIMAGINA_LINUX_PORT=ON ..` then `make -j8` produces an `Imagina`
binary that links and runs. The current binary is just a stub that
prints a message and exits; the actual rendering is not wired yet.

### What works

- CMake detects GLFW3 / GMP / GMPXX / libpng / freetype2 / OpenGL via pkg-config.
- Compute / math core compiles clean: `Computation`, `Evaluator`,
  `HInfLAEvaluator`, `FractalContext`, `JitEvaluator`, `FeatureFinder`,
  `Global`, `PlatformDependent`.

### What is excluded for Phase 1

| File                   | Why                                                      | Phase to re-add |
|------------------------|----------------------------------------------------------|------|
| `main.cpp`             | `wWinMain` + Win32 message loop                          | 2    |
| `MainWindow.cpp`       | 1439 lines of WndProc, menus, dialogs, wgl context       | 3    |
| `CLI.cpp`              | `HWND`, `MessageBoxW`, `PostQuitMessage`                 | 2    |
| `Overlay.cpp`          | GDI text rendering                                       | 2    |
| `File.cpp`             | `wchar_t` `ifstream`/`fopen`, `_wfopen_s`, dialog popups | 2    |
| `GLUtils.cpp`          | `wglGetProcAddress` extension loader                     | 2    |
| `Render.cpp`           | `SwapBuffers`, heavy GL state                            | 2    |
| `PixelManager.cpp`     | GL texture pipeline                                      | 2    |

A `LinuxStubs.cpp` provides the linker symbols (`PixelManager`,
`StandardPixelManager`) that the math core references so it can build
in isolation.

### Key adaptations

- `LinuxCompat.h`: `__forceinline` → `inline __attribute__((always_inline))`,
  `sscanf_s` → `sscanf`, pulls in `<sys/sysinfo.h>`.
- `Includes.h` / `ArbitraryPrecision.h` / `FloatExp.h` / `Vector4.h` /
  `FloatExpVector4.h`: `<mpirxx.h>` → `<gmpxx.h>` and `<intrin.h>` →
  `<x86intrin.h>` under `#ifdef IMAGINA_LINUX`.
- `Types.h`: `VirtualAlloc`/`VirtualFree` replaced by `mmap`+`madvise`
  via `ImaginaReserveVirtual`/`ImaginaCommitVirtual`/etc. helpers.
- `Types.h::operator""_hp`: `unsigned long long` cast to `unsigned long`
  so the GMP `__gmp_expr` constructor disambiguates.
- `FloatExpVector4.h`: anonymous union with non-trivial members
  (GCC-rejected) replaced by `DExpVec4NormalizedPositiveView` + a
  `_get_NormalizedPositive()` method (with a `#define` alias so call
  sites stay `.AssumeNormalizedPositive`).
- `HInfLAEvaluator.h::Temp`: explicit member-template specialization
  (illegal in non-namespace scope) converted to partial specialization
  via a dummy `int = 0` template parameter.
- `HInfLAEvaluator.cpp` / `Evaluator.cpp`: `MEMORYSTATUSEX` /
  `GlobalMemoryStatusEx` replaced by `sysinfo()`.
- `HInfLAEvaluator.cpp`: `__declspec(noinline)` wrapped behind
  `IM_NOINLINE` (`__attribute__((noinline))` on Linux).
- `Evaluator.cpp::operator^`: `__forceinline` weakened to `inline` on
  Linux because GCC refuses to inline a switch-based function.
- `Computation.cpp`: Windows thread-priority / affinity calls wrapped
  in `#ifdef _WIN32`. Linux just calls `SetWorkerPriority` (Linux
  variant pins `SCHED_IDLE`).
- `FractalContext.cpp`: `std::min(0ll, int64_t)` typed via explicit
  template parameter; `HRReal(1ull << 20)` cast to `unsigned long` to
  pick the right `FExpDouble` constructor.
- GCC compat flags: `-fms-extensions -fpermissive -Wno-permissive`.

## Phase 2 (next): Build a real entrypoint

Replace `main_linux.cpp` with a real entrypoint that:

1. Creates a GLFW window + OpenGL 3.3 core context (replaces the wgl
   path in `MainWindow.cpp` and the SwapBuffers call in `Render.cpp`).
2. Loads GL extensions via `glfwGetProcAddress` (replaces
   `wglGetProcAddress` in `GLUtils.cpp`).
3. Re-enables `Render.cpp`, `PixelManager.cpp`, `GLUtils.cpp` (probably
   with minor `#ifdef` patches to skip the Win32 init paths inside).
4. Re-enables `File.cpp` with `std::filesystem::path` instead of raw
   `wchar_t *`, and `fopen`/`fstream` instead of `_wfopen_s`.
5. Re-enables `CLI.cpp` with platform-abstracted error reporting (no
   `MessageBoxW`; print to stderr or feed into the GUI).
6. Re-enables `Overlay.cpp` rewritten against FreeType for text
   rendering instead of GDI.

## Phase 3 (later): GUI

Port `MainWindow.cpp` to GLFW window events + Dear ImGui for menus and
dialogs. 23 menu items and 8 dialogs need to be reproduced. Estimated
2-4 weeks of focused work.

## Phase 4 (polish): File dialogs, drag-and-drop, accelerators.

Likely via `nativefiledialog-extended` or `tinyfiledialogs`.
