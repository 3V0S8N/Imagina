# Linux Port — Progress

## Phase 2 (DONE): Imagina renders on Linux

```bash
cmake -DIMAGINA_LINUX_PORT=ON ..
make -j8
./Imagina --auto-render --output mandelbrot.png --width 1024 --height 1024
./Imagina --render-zoom --output-dir frames/ --frames 300 --zoom-step 0.97
./Imagina --help
```

All three CLI modes work. PNG output is bit-equivalent to the Windows build
modulo overlay text (currently disabled on Linux — Phase 3).

### What works on Linux

- GLFW window + OpenGL 3.3 compat context
- Full GL render pipeline (`Render.cpp`, `PixelManager.cpp`, `GLUtils.cpp`)
- `--auto-render` single-still PNG output
- `--render-zoom` frame-sequence output (uses corrected POSIX path separator)
- `--help` (printed to stderr via `MessageBoxW` shim)
- File loading (`File.cpp`): `.im`, `.imt`, `.kfr` via UTF-8 paths
- `AutoBumpItLim` (after merging master's `more aids` fix)
- Math compute core: `Computation`, `Evaluator`, `HInfLAEvaluator`,
  `FractalContext`, `JitEvaluator`, `FeatureFinder`

### What does NOT work yet on Linux

- **Interactive input**: mouse panning, scroll zoom, keyboard shortcuts.
  Wired only as no-op callbacks in `main_linux.cpp` — needs the input
  routing code from `MainWindow.cpp`'s `WindowProcess` to be ported.
- **Menus**: 23 menu items live in `MainWindow.cpp`. Phase 3 → ImGui.
- **Dialog boxes**: 8 Win32 dialogs (Location, Iteration Limit, etc.).
  Phase 3 → ImGui modals.
- **File-open dialog**: `MainWindow.cpp` uses `GetOpenFileNameW`.
  Phase 3 → `nativefiledialog-extended` or `tinyfiledialogs`.
- **In-image text overlay**: `DrawTextOverlay` is a no-op on Linux.
  Phase 3 → FreeType.

### Linux-specific files & wrappers

| File | Role |
|---|---|
| `main_linux.cpp` | GLFW window + GL context + CLI integration + main loop |
| `LinuxCompat.h` | `__forceinline`, `sscanf_s`, `MessageBoxW`, `_wfopen_s`, `imagina_wchar_to_utf8` |
| `LinuxStubs.cpp` | Placeholder (empty, kept for CMake globbing) |
| `PlatformDependent.cpp` | `SetWorkerPriority` (SCHED_IDLE) + `ErrorMessage` → stderr |

### Key adaptations recap

- `<mpirxx.h>` → `<gmpxx.h>`
- `<intrin.h>` → `<x86intrin.h>`
- `VirtualAlloc`/`Free` → `mmap`/`munmap` via `Imagina*Virtual` helpers
- `MEMORYSTATUSEX`/`GlobalMemoryStatusEx` → `sysinfo()`
- `__declspec(noinline)` → `__attribute__((noinline))` (`IM_NOINLINE`)
- `__forceinline` → `inline __attribute__((always_inline))`
- `__debugbreak` → `__builtin_trap`
- `wglGetProcAddress` → not needed (libGL exports symbols directly)
- `SwapBuffers(MainDC)` → `glfwSwapBuffers(g_glfw_window)`
- `wglSwapIntervalEXT` → forwards to `glfwSwapInterval`
- `PostQuitMessage` → `glfwSetWindowShouldClose`
- `SetWindowTextW` → `glfwSetWindowTitle` (UTF-8 conversion)
- `MessageBoxW` → `fprintf(stderr, ...)`
- `_wfopen_s` → `fopen` with UTF-8 path
- `std::ifstream(wchar_t*)` → `std::ifstream(utf8 string)`
- Win32 thread priority/affinity → silently skipped (we use `SCHED_IDLE` instead)
- `template<>` member specialization → partial specialization with dummy `int = 0`
- Anonymous union with non-trivial members → named view struct + reinterpret-cast accessor
- GCC compat flags: `-fms-extensions -fpermissive -Wno-permissive`
- Path separator: `\` → `/` in `CheckFrameSequenceProgress`
- Locale: `setlocale(LC_NUMERIC, "C")` so CLI decimal parsing works on de_DE

## Phase 3 (next): Interactive GUI

1. Port `WindowProcess` input handling from `MainWindow.cpp` to GLFW
   callbacks (mouse, scroll, keyboard).
2. Add Dear ImGui for menus + dialogs (replaces the 23 menu items and 8
   dialogs).
3. Add `tinyfiledialogs` (or `nfd-extended`) for file open/save dialogs.
4. Implement `DrawTextOverlay` with FreeType for in-image zoom labels.

Estimated 2-3 weeks of focused work for full GUI parity.

## Phase 4 (polish)

- Drag-and-drop of `.im` files onto the window.
- Right-click context menu.
- HiDPI scaling.
