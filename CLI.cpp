#include "Includes.h"
#include "CLI.h"
#include "File.h"
#include "Overlay.h"
#include <chrono>

void OpenFile(wchar_t *FileName, size_t ExtensionOffset = 0);
void SaveImage(wchar_t *FileName);

extern HWND HWnd;

// Build overlay text.
static void BuildOverlayText() {
	const auto &HalfH = FContext.CurrentLocation.HalfH;
	double mant = double(HalfH.Mantissa);
	if (mant <= 0.0 || !std::isfinite(mant)) { g_overlay_text.clear(); return; }
	const double LOG10_2 = 0.30102999566398119521;
	double log10_zoom = double(1 - HalfH.Exponent) * LOG10_2 - log10(mant);
	double log_floor = floor(log10_zoom);
	double display_mant = pow(10.0, log10_zoom - log_floor);
	if (display_mant >= 10.0) { display_mant /= 10.0; log_floor += 1.0; }
	if (display_mant < 1.0)   { display_mant *= 10.0; log_floor -= 1.0; }

	wchar_t buf[256];
	swprintf(buf, 256, L"Zoom %.3fe%+lld   Iter %llu",
	    display_mant, (long long)log_floor,
	    (unsigned long long)Global::ItLim);
	g_overlay_text = buf;
}

CLIArgs g_cli;

static bool WStrEq(const wchar_t *a, const wchar_t *b) {
	return wcscmp(a, b) == 0;
}

static bool ParseUInt(const wchar_t *s, uint64_t &out) {
	if (!s || !*s) return false;
	wchar_t *end = nullptr;
	unsigned long long v = wcstoull(s, &end, 10);
	if (end == s || *end != 0) return false;
	out = (uint64_t)v;
	return true;
}

void ParseCLI(int argc, wchar_t **argv) {
	// Skip argv[0].
	for (int i = 1; i < argc; i++) {
		const wchar_t *a = argv[i];

		auto need_value = [&](const wchar_t *name) -> const wchar_t * {
			if (i + 1 >= argc) {
				g_cli.valid = false;
				g_cli.error = std::wstring(L"Missing value after ") + name;
				return nullptr;
			}
			return argv[++i];
		};

		if (WStrEq(a, L"--help") || WStrEq(a, L"-h") || WStrEq(a, L"/?")) {
			g_cli.valid = false; // help sentinel.
			g_cli.error = L"help";
			return;
		} else if (WStrEq(a, L"--location") || WStrEq(a, L"-l")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			g_cli.location_file = v;
		} else if (WStrEq(a, L"--width") || WStrEq(a, L"-w")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			uint64_t n;
			if (!ParseUInt(v, n) || n < 16 || n > 65536) {
				g_cli.valid = false;
				g_cli.error = L"Invalid --width (16..65536)";
				return;
			}
			g_cli.width = (int)n;
		} else if (WStrEq(a, L"--height") || WStrEq(a, L"-H")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			uint64_t n;
			if (!ParseUInt(v, n) || n < 16 || n > 65536) {
				g_cli.valid = false;
				g_cli.error = L"Invalid --height (16..65536)";
				return;
			}
			g_cli.height = (int)n;
		} else if (WStrEq(a, L"--iterations") || WStrEq(a, L"-i")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			if (!ParseUInt(v, g_cli.iterations) || g_cli.iterations == 0) {
				g_cli.valid = false;
				g_cli.error = L"Invalid --iterations";
				return;
			}
		} else if (WStrEq(a, L"--output") || WStrEq(a, L"-o")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			g_cli.output_file = v;
		} else if (WStrEq(a, L"--center-x")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			wchar_t *end = nullptr;
			double d = wcstod(v, &end);
			if (end == v || *end != 0) {
				g_cli.valid = false; g_cli.error = L"Invalid --center-x"; return;
			}
			g_cli.center_x = d;
			g_cli.has_center = true;
		} else if (WStrEq(a, L"--center-y")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			wchar_t *end = nullptr;
			double d = wcstod(v, &end);
			if (end == v || *end != 0) {
				g_cli.valid = false; g_cli.error = L"Invalid --center-y"; return;
			}
			g_cli.center_y = d;
			g_cli.has_center = true;
		} else if (WStrEq(a, L"--zoom")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			wchar_t *end = nullptr;
			double d = wcstod(v, &end);
			if (end == v || *end != 0 || d <= 0.0) {
				g_cli.valid = false; g_cli.error = L"Invalid --zoom"; return;
			}
			g_cli.zoom_start = d;
		} else if (WStrEq(a, L"--auto-render")) {
			g_cli.auto_render = true;
		} else if (WStrEq(a, L"--render-zoom")) {
			g_cli.render_zoom = true;
		} else if (WStrEq(a, L"--frames")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			if (!ParseUInt(v, g_cli.frames_total) || g_cli.frames_total == 0) {
				g_cli.valid = false;
				g_cli.error = L"Invalid --frames";
				return;
			}
		} else if (WStrEq(a, L"--zoom-step")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			wchar_t *end = nullptr;
			double d = wcstod(v, &end);
			if (end == v || *end != 0 || d <= 0.0 || d >= 100.0) {
				g_cli.valid = false;
				g_cli.error = L"Invalid --zoom-step (0..100)";
				return;
			}
			g_cli.zoom_step = d;
		} else if (WStrEq(a, L"--output-dir")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			g_cli.frames_dir = v;
		} else if (WStrEq(a, L"--start-frame")) {
			const wchar_t *v = need_value(a);
			if (!v) return;
			if (!ParseUInt(v, g_cli.frame_index)) {
				g_cli.valid = false;
				g_cli.error = L"Invalid --start-frame";
				return;
			}
		} else if (a[0] == L'-') {
			g_cli.valid = false;
			g_cli.error = std::wstring(L"Unknown option: ") + a;
			return;
		} else {
			// Positional = location file.
			if (g_cli.location_file.empty()) {
				g_cli.location_file = a;
			} else {
				g_cli.valid = false;
				g_cli.error = std::wstring(L"Unexpected positional arg: ") + a;
				return;
			}
		}
	}

	// Validation.
	if (g_cli.auto_render && g_cli.output_file.empty()) {
		g_cli.valid = false;
		g_cli.error = L"--auto-render requires --output <file.png>";
	}
	if (g_cli.auto_render && g_cli.render_zoom) {
		g_cli.valid = false;
		g_cli.error = L"--auto-render and --render-zoom are mutually exclusive";
	}
	if (g_cli.render_zoom) {
		if (g_cli.frames_dir.empty()) {
			g_cli.valid = false;
			g_cli.error = L"--render-zoom requires --output-dir <dir>";
		} else if (g_cli.frames_total == 0) {
			g_cli.valid = false;
			g_cli.error = L"--render-zoom requires --frames <N>";
		} else if (g_cli.frame_index >= g_cli.frames_total) {
			g_cli.valid = false;
			g_cli.error = L"--start-frame must be < --frames";
		}
	}
}

// Forward declaration so ApplyCLISettings can call it.
static void AutoBumpItLim();

void ApplyCLISettings() {
	// Load location file.
	if (!g_cli.location_file.empty()) {
		OpenFile((wchar_t *)g_cli.location_file.c_str());
	}

	// CLI center overrides file.
	if (g_cli.has_center || g_cli.zoom_start > 0.0) {
		if (g_cli.has_center) {
			FContext.CenterCoordinate.X = mpf_class(g_cli.center_x);
			FContext.CenterCoordinate.Y = mpf_class(g_cli.center_y);
		}
		HRReal newHalfH = (g_cli.zoom_start > 0.0)
		    ? HRReal(2.0 / g_cli.zoom_start)
		    : FContext.CurrentLocation.HalfH;
		FContext.CurrentLocation.HalfH = newHalfH;
		FContext.CurrentLocation.X = 0.0_hr;
		FContext.CurrentLocation.Y = 0.0_hr;
		FContext.RenderLocation = FContext.CurrentLocation;
		FContext.EvalLocation = FContext.CurrentLocation;
		FContext.InvalidateAll();
	}

	// Iteration limit.
	if (g_cli.iterations > 0) {
		Global::ItLim = (size_t)g_cli.iterations;
		FContext.ParameterChanged = true;
	}

	// Resolution.
	if (g_cli.width > 0 && g_cli.height > 0) {
		FContext.ImageWidth = (size_t)g_cli.width;
		FContext.ImageHeight = (size_t)g_cli.height;
		FContext.pixelManager.SetResolution(g_cli.width, g_cli.height);
		FContext.ParameterChanged = true;
		FContext.RecomputeReference = true;
	}

	// Resume: fast-forward zoom.
	if (g_cli.render_zoom && g_cli.frame_index > 0) {
		RelLocation NewLoc = FContext.CurrentLocation;
		HRReal step = HRReal(g_cli.zoom_step);
		for (uint64_t i = 0; i < g_cli.frame_index; i++) {
			NewLoc.HalfH = NewLoc.HalfH * step;
		}
		FContext.Zooming = false;
		FContext.ChangeLocation(NewLoc);
	}

	if (g_cli.render_zoom || g_cli.auto_render) {
		AutoBumpItLim();
	}
}

// Is renderer fully settled?
static bool IsRenderFullySettled() {
	if (!FContext.pixelManager.Completed()) return false;
	if (FContext.ReferenceTaskContext) return false;
	if (FContext.ParameterChanged) return false;
	if (FContext.RecomputeReference) return false;
	if (FContext.ComputePixel) return false;
	if (FContext.Zooming) return false;
	return true;
}

// Auto-grow iteration limit with zoom depth.
static void AutoBumpItLim() {
	const auto &HalfH = FContext.CurrentLocation.HalfH;
	double mant = double(HalfH.Mantissa);
	if (mant <= 0.0 || !std::isfinite(mant)) return;
	const double LOG10_2 = 0.30102999566398119521;
	double log10_zoom = double(1 - HalfH.Exponent) * LOG10_2 - log10(mant);
	if (log10_zoom < 0) log10_zoom = 0;
	size_t target = (size_t)(2048.0 * log10_zoom + 1024.0);
	size_t floor_iter = (g_cli.iterations > 0) ? (size_t)g_cli.iterations : 1024;
	if (target < floor_iter) target = floor_iter;
	target = std::min<size_t>(target, (size_t)1 << 48);
	if (Global::ItLim < target) {
		Global::ItLim = target;
		FContext.ParameterChanged = true;
		FContext.RecomputeReference = true;
	}
}

bool CheckFrameSequenceProgress() {
	if (!g_cli.render_zoom) return false;
	if (g_cli.frame_index >= g_cli.frames_total) return false;
	if (!IsRenderFullySettled()) return false;

	// Save frame.
	wchar_t path[1024];
	swprintf(path, 1024, L"%ls\\frame_%05llu.png",
	         g_cli.frames_dir.c_str(),
	         (unsigned long long)g_cli.frame_index);
	BuildOverlayText();
	SaveImage(path);
	g_overlay_text.clear();
	g_cli.frame_index++;

	// Done?
	if (g_cli.frame_index >= g_cli.frames_total) {
		PostQuitMessage(0);
		return true;
	}

	// Step zoom.
	RelLocation NewLoc = FContext.CurrentLocation;
	NewLoc.HalfH *= HRReal(g_cli.zoom_step);
	FContext.Zooming = false;
	FContext.ChangeLocation(NewLoc);
	AutoBumpItLim();

	return false;
}

bool CheckAutoRenderComplete() {
	if (!g_cli.auto_render || g_cli.render_done) return false;
	if (!IsRenderFullySettled()) return false;

	// Save and quit.
	BuildOverlayText();
	SaveImage((wchar_t *)g_cli.output_file.c_str());
	g_overlay_text.clear();
	g_cli.render_done = true;

	PostQuitMessage(0);
	return true;
}

void UpdateWindowTitle() {
	using clock = std::chrono::steady_clock;
	static clock::time_point last_update{};
	auto now = clock::now();
	if (last_update != clock::time_point{} &&
	    (now - last_update) < std::chrono::milliseconds(200)) return;
	last_update = now;

	if (!HWnd) return;
	if (!Global::Initialized) return;

	// log10(mag) from HalfH = mant * 2^exp.
	const auto &HalfH = FContext.CurrentLocation.HalfH;
	double mant = double(HalfH.Mantissa);
	if (mant <= 0.0 || !std::isfinite(mant)) return;
	const double LOG10_2 = 0.30102999566398119521;
	double log10_zoom = double(1 - HalfH.Exponent) * LOG10_2 - log10(mant);

	double log_floor = floor(log10_zoom);
	double display_mant = pow(10.0, log10_zoom - log_floor);
	if (display_mant >= 10.0) { display_mant /= 10.0; log_floor += 1.0; }
	if (display_mant < 1.0)   { display_mant *= 10.0; log_floor -= 1.0; }

	wchar_t buf[256];
	if (g_cli.render_zoom) {
		swprintf(buf, 256,
		    L"Imagina - Zoom %.3fe%+lld - Iter %llu - Frame %llu/%llu",
		    display_mant, (long long)log_floor,
		    (unsigned long long)Global::ItLim,
		    (unsigned long long)g_cli.frame_index,
		    (unsigned long long)g_cli.frames_total);
	} else if (g_cli.auto_render) {
		swprintf(buf, 256,
		    L"Imagina - Zoom %.3fe%+lld - Iter %llu - Auto-render",
		    display_mant, (long long)log_floor,
		    (unsigned long long)Global::ItLim);
	} else {
		swprintf(buf, 256,
		    L"Imagina - Zoom %.3fe%+lld - Iter %llu",
		    display_mant, (long long)log_floor,
		    (unsigned long long)Global::ItLim);
	}
	SetWindowTextW(HWnd, buf);
}

void ShowHelp() {
	const wchar_t *msg =
		L"Imagina CLI options:\n\n"
		L"  --location <file>     Load .im/.imt/.kfr at startup\n"
		L"  --width <N>           Image width in pixels\n"
		L"  --height <N>          Image height in pixels\n"
		L"  --iterations <N>      Max iteration limit\n\n"
		L"Mode A - single still:\n"
		L"  --output <file.png>   Output PNG path\n"
		L"  --auto-render         Render single image then exit\n\n"
		L"Starting center / zoom (overrides loaded location):\n"
		L"  --center-x <X>        Center real part (e.g. -0.743643887)\n"
		L"  --center-y <Y>        Center imaginary part (e.g. 0.131825904)\n"
		L"  --zoom <M>            Starting magnification (1 = default, 1e6 = 1M-x)\n\n"
		L"Mode B - frame sequence (zoom video):\n"
		L"  --render-zoom         Enable frame-sequence mode\n"
		L"  --output-dir <dir>    Frame output directory\n"
		L"  --frames <N>          Total frame count\n"
		L"  --zoom-step <S>       HalfH multiplier per frame (default 0.97)\n"
		L"  --start-frame <N>     Resume from frame N (0-based)\n\n"
		L"  --help                Show this message\n\n"
		L"Example zoom-in over 1800 frames:\n"
		L"  Imagina.exe --location deep.im --width 2560 --height 1440 \\\n"
		L"              --iterations 100000 --output-dir frames/ \\\n"
		L"              --frames 1800 --zoom-step 0.97 --render-zoom";

	MessageBoxW(nullptr, msg, L"Imagina", MB_OK | MB_ICONINFORMATION);
}
