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

### What now works (Phase 3.5 done)

- **Mouse**: left-drag = pan, scroll = zoom in/out at cursor
- **Keyboard shortcuts**: A/S = color density, D/F = iter limit, E/R = color cycling,
  Ctrl+O = open location, Ctrl+S = save location, Ctrl+Shift+S = save image,
  F11 = fullscreen toggle, Esc = leave fullscreen
- **File-open / save dialogs**: vendored `tinyfiledialogs` in `third_party/`
  (zlib license). Routes to zenity/kdialog/xmessage on Linux.
- **In-image text overlay**: FreeType (DejaVu Sans Mono Bold) renders the
  `Zoom 1.000e+0   Iter 1024` line in the same dark-backdrop style as
  Windows GDI.

### What still does NOT work on Linux

- **Menus**: 23 menu items in `MainWindow.cpp`. Phase 4 → ImGui.
- **Modal dialogs** for fine parameter input: 8 Win32 dialogs (Location,
  Iteration Limit, etc.). Phase 4 → ImGui modals.
  (Workaround: dial parameters via keyboard + click-zoom.)

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

## Phase 4 (optional): Menus and modal dialogs via ImGui

Only thing the current Linux build lacks vs Windows: a menu bar and
8 modal parameter-input dialogs. Workaround until then is keyboard
shortcuts + Ctrl+O / Ctrl+S, which already cover the common workflow.

Estimated 1-2 weeks of focused work.

## Phase 4 (polish)

- Drag-and-drop of `.im` files onto the window.
- Right-click context menu.
- HiDPI scaling.
