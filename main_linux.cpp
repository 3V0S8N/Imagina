// Linux entrypoint -- Phase 2.
// Creates a GLFW window + OpenGL context, runs the main render loop.
// CLI parsing and File I/O still missing -- Phase 2.5.

#include "Includes.h"
#include "Render.h"
#include "Global.h"
#include "CLI.h"

#include "File.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include "tinyfiledialogs.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

void OpenFile(wchar_t *FileName, size_t ExtensionOffset = 0);
void SaveImage(wchar_t *FileName);
void SaveFile(wchar_t *FileName, FileType Type);

// Forward declarations of globals defined below (so the menu-bar code can use them).
extern GLFWwindow *g_glfw_window;
extern bool UseDE;
extern bool UseLinearApproximation;
extern bool UsePalleteMipmaps;
void SetFractalType(FractalTypeEnum Type);
extern FractalTypeEnum CurrentFractalType;
static void toggle_fullscreen(GLFWwindow *w);

static std::wstring utf8_to_wide(const char *s) {
	if (!s) return {};
	std::mbstate_t state{};
	std::wstring out;
	while (*s) {
		wchar_t wc;
		size_t consumed = std::mbrtowc(&wc, s, MB_CUR_MAX, &state);
		if (consumed == size_t(-1) || consumed == size_t(-2)) {
			wc = (unsigned char)*s; consumed = 1; state = std::mbstate_t{};
		} else if (consumed == 0) {
			break;
		}
		out.push_back(wc);
		s += consumed;
	}
	return out;
}

static void open_location_dialog() {
	const char *patterns[] = {"*.im", "*.imt", "*.kfr", "*.kfp"};
	const char *path = tinyfd_openFileDialog(
		"Open Imagina location", "", 4, patterns,
		"Imagina locations (*.im, *.imt, *.kfr, *.kfp)", 0);
	if (!path) return;
	std::wstring wpath = utf8_to_wide(path);
	// Find extension offset: position of last '.' in path.
	size_t dot = wpath.find_last_of(L'.');
	size_t ext_off = (dot == std::wstring::npos) ? 0 : (dot + 1);
	OpenFile(wpath.data(), ext_off);
}

static void save_image_dialog() {
	const char *patterns[] = {"*.png"};
	const char *path = tinyfd_saveFileDialog(
		"Save image as PNG", "imagina.png", 1, patterns,
		"PNG image (*.png)");
	if (!path) return;
	std::wstring wpath = utf8_to_wide(path);
	SaveImage(wpath.data());
}

static void save_location_dialog() {
	const char *patterns[] = {"*.im"};
	const char *path = tinyfd_saveFileDialog(
		"Save location", "imagina.im", 1, patterns,
		"Imagina location (*.im)");
	if (!path) return;
	std::wstring wpath = utf8_to_wide(path);
	SaveFile(wpath.data(), FileType::Imagina);
}

static bool g_is_fullscreen = false;
static int g_windowed_x = 0, g_windowed_y = 0;
static int g_windowed_w = 0, g_windowed_h = 0;

// === Dear ImGui menu bar ============================================
static bool g_show_iter_dialog = false;
static bool g_show_about       = false;
static int  g_iter_dialog_buf  = 0;

static void reset_location() {
	FContext.CurrentLocation = { 0.0, 0.0, 2.0 };
	FContext.RenderLocation  = FContext.CurrentLocation;
	FContext.EvalLocation    = FContext.CurrentLocation;
	FContext.InvalidateAll();
}

static void set_fractal_via_menu(FractalTypeEnum t) {
	SetFractalType(t);
	FContext.InvalidateAll();
}

static void draw_menu_bar() {
	if (!ImGui::BeginMainMenuBar()) return;
	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Open location...",  "Ctrl+O"))       open_location_dialog();
		if (ImGui::MenuItem("Save location...",  "Ctrl+S"))       save_location_dialog();
		if (ImGui::MenuItem("Save image (PNG)...", "Ctrl+Shift+S")) save_image_dialog();
		ImGui::Separator();
		if (ImGui::MenuItem("Quit"))             glfwSetWindowShouldClose(g_glfw_window, GLFW_TRUE);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Fractal")) {
		if (ImGui::BeginMenu("Formula")) {
			if (ImGui::MenuItem("Mandelbrot",   nullptr, CurrentFractalType == FractalTypeEnum::Mandelbrot))   set_fractal_via_menu(FractalTypeEnum::Mandelbrot);
			if (ImGui::MenuItem("Tricorn",      nullptr, CurrentFractalType == FractalTypeEnum::Tricorn))      set_fractal_via_menu(FractalTypeEnum::Tricorn);
			if (ImGui::MenuItem("Burning ship", nullptr, CurrentFractalType == FractalTypeEnum::BurningShip)) set_fractal_via_menu(FractalTypeEnum::BurningShip);
			if (ImGui::MenuItem("Nova",         nullptr, CurrentFractalType == FractalTypeEnum::Nova))         set_fractal_via_menu(FractalTypeEnum::Nova);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Distance estimation", nullptr, UseDE)) { UseDE = !UseDE; FContext.ParameterChanged = true; }
		ImGui::Separator();
		if (ImGui::MenuItem("Reset location"))                 reset_location();
		if (ImGui::MenuItem("Iteration limit..."))             { g_show_iter_dialog = true; g_iter_dialog_buf = (int)Global::ItLim; }
		ImGui::Separator();
		if (ImGui::MenuItem("Increase color density (A)"))     { Global::ItDiv = std::max(8.0_sr,    Global::ItDiv / 2); Global::Redraw = true; }
		if (ImGui::MenuItem("Decrease color density (S)"))     { Global::ItDiv = std::min(0x1p48_sr, Global::ItDiv * 2); Global::Redraw = true; }
		if (ImGui::MenuItem("Increase iteration limit (D)"))   { Global::ItLim = std::min(UINT64_C(1) << 48, Global::ItLim * 2); FContext.ParameterChanged = true; FContext.RecomputeReference = true; }
		if (ImGui::MenuItem("Decrease iteration limit (F)"))   { Global::ItLim = std::max(UINT64_C(8),       Global::ItLim / 2); FContext.ParameterChanged = true; }
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Computation")) {
		if (ImGui::MenuItem("Linear approximation", nullptr, UseLinearApproximation)) { UseLinearApproximation = !UseLinearApproximation; FContext.RecomputeReference = true; }
		if (ImGui::MenuItem("Lock reference",       nullptr, FContext.LockReference)) { FContext.LockReference   = !FContext.LockReference; }
		ImGui::Separator();
		if (ImGui::MenuItem("Recompute reference")) { FContext.RecomputeReference = true; }
		if (ImGui::MenuItem("Recompute all"))       { FContext.InvalidateAll(); }
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Image")) {
		if (ImGui::MenuItem("Toggle fullscreen", "F11")) toggle_fullscreen(g_glfw_window);
		if (ImGui::MenuItem("Palette mipmaps", nullptr, UsePalleteMipmaps)) { UsePalleteMipmaps = !UsePalleteMipmaps; Global::Redraw = true; }
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Help")) {
		if (ImGui::MenuItem("About Imagina (Linux port)")) g_show_about = true;
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();
}

static void draw_iter_dialog() {
	if (!g_show_iter_dialog) return;
	ImGui::SetNextWindowSize(ImVec2(320, 120), ImGuiCond_Once);
	if (ImGui::Begin("Iteration limit", &g_show_iter_dialog,
		ImGuiWindowFlags_NoCollapse)) {
		ImGui::InputInt("Iterations", &g_iter_dialog_buf, 1024, 16384);
		if (g_iter_dialog_buf < 8) g_iter_dialog_buf = 8;
		if (ImGui::Button("Apply")) {
			Global::ItLim = (uint64_t)g_iter_dialog_buf;
			FContext.ParameterChanged = true;
			FContext.RecomputeReference = true;
			g_show_iter_dialog = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) g_show_iter_dialog = false;
	}
	ImGui::End();
}

static void draw_about() {
	if (!g_show_about) return;
	ImGui::SetNextWindowSize(ImVec2(400, 180), ImGuiCond_Once);
	if (ImGui::Begin("About", &g_show_about,
		ImGuiWindowFlags_NoCollapse)) {
		ImGui::Text("Imagina -- Linux Port");
		ImGui::Separator();
		ImGui::TextWrapped("Fractal renderer built on perturbation theory + linear approximation. "
			"Original Windows code by Zhuoran. Linux port via GLFW + ImGui + FreeType + GMP.");
		ImGui::Separator();
		if (ImGui::Button("Close")) g_show_about = false;
	}
	ImGui::End();
}

static void toggle_fullscreen(GLFWwindow *w) {
	if (!g_is_fullscreen) {
		glfwGetWindowPos(w, &g_windowed_x, &g_windowed_y);
		glfwGetWindowSize(w, &g_windowed_w, &g_windowed_h);
		GLFWmonitor *mon = glfwGetPrimaryMonitor();
		const GLFWvidmode *mode = glfwGetVideoMode(mon);
		glfwSetWindowMonitor(w, mon, 0, 0, mode->width, mode->height, mode->refreshRate);
		g_is_fullscreen = true;
	} else {
		glfwSetWindowMonitor(w, nullptr, g_windowed_x, g_windowed_y, g_windowed_w, g_windowed_h, 0);
		g_is_fullscreen = false;
	}
}

// Win32 globals normally defined in MainWindow.cpp. Other translation units
// reach them by extern; we define them as null stubs on Linux.
void *HWnd     = nullptr;
void *MainDC   = nullptr;
void *GLContext = nullptr;

size_t InitialWindowWidth  = 2048;
size_t InitialWindowHeight = 1024;
size_t WindowWidth         = 2048;
size_t WindowHeight        = 1024;
bool   FullScreen          = false;
int32_t MouseX             = 0;
int32_t MouseY             = 0;
bool   Panning             = false;

namespace Global {
	bool moved = false;
	bool ImageSizeFollowWindowSize = true;
}

// CurrentFractalType is defined in Global.cpp. SetFractalType normally lives in MainWindow.cpp.
extern FractalTypeEnum CurrentFractalType;
void SetFractalType(FractalTypeEnum Type) { CurrentFractalType = Type; }

// MainWindow.cpp normally owns these toggle globals.
bool UseDE = true;
bool UseLinearApproximation = true;
bool UsePalleteMipmaps = false;

// Exposed to Render.cpp so EndRender can call glfwSwapBuffers on the right window.
GLFWwindow *g_glfw_window = nullptr;

static void glfw_error_cb(int code, const char *msg) {
	std::fprintf(stderr, "GLFW error %d: %s\n", code, msg);
}

static void framebuffer_size_cb(GLFWwindow * /*w*/, int width, int height) {
	WindowWidth  = (size_t)width;
	WindowHeight = (size_t)height;
	Global::SizeChanged = true;
}

static void cursor_pos_cb(GLFWwindow * /*w*/, double x, double y) {
	int32_t nx = (int32_t)x;
	int32_t ny = (int32_t)y;
	if (Panning) {
		double MovementX = -double(nx - MouseX) / WindowHeight * 2.0;
		double MovementY =  double(ny - MouseY) / WindowHeight * 2.0;
		double FractalMovementX = Global::InvTransformMatrix[0][0] * MovementX + Global::InvTransformMatrix[1][0] * MovementY;
		double FractalMovementY = Global::InvTransformMatrix[0][1] * MovementX + Global::InvTransformMatrix[1][1] * MovementY;
		Global::moved = true;
		FContext.Move(FractalMovementX, FractalMovementY);
	}
	MouseX = nx;
	MouseY = ny;
	Global::Redraw = true;
}

static void mouse_button_cb(GLFWwindow *w, int button, int action, int /*mods*/) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		Panning = (action == GLFW_PRESS);
		double cx, cy;
		glfwGetCursorPos(w, &cx, &cy);
		MouseX = (int32_t)cx;
		MouseY = (int32_t)cy;
	}
}

static void scroll_cb(GLFWwindow *w, double /*xoffset*/, double yoffset) {
	double cx, cy;
	glfwGetCursorPos(w, &cx, &cy);
	SRReal X =  (SRReal((int)cx) - (double)WindowWidth  * 0.5) / WindowHeight * 2.0;
	SRReal Y = -(SRReal((int)cy) / WindowHeight * 2.0 - 1.0);
	SRReal FractalX = Global::InvTransformMatrix[0][0] * X + Global::InvTransformMatrix[1][0] * Y;
	SRReal FractalY = Global::InvTransformMatrix[0][1] * X + Global::InvTransformMatrix[1][1] * Y;
	if (yoffset > 0) FContext.ZoomIn(FractalX, FractalY);
	else if (yoffset < 0) FContext.ZoomOut(FractalX, FractalY);
}

static void key_cb(GLFWwindow *w, int key, int /*scancode*/, int action, int mods) {
	if (action != GLFW_PRESS && action != GLFW_REPEAT) {
		if (action == GLFW_RELEASE && (key == GLFW_KEY_E || key == GLFW_KEY_R)) {
			Global::ColorCyclingSpeed = 0.0;
			Global::Redraw = true;
		}
		return;
	}
	const bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
	const bool shift = (mods & GLFW_MOD_SHIFT)   != 0;
	if (ctrl) {
		switch (key) {
			case GLFW_KEY_O: open_location_dialog(); return;
			case GLFW_KEY_S:
				if (shift) save_image_dialog();
				else       save_location_dialog();
				return;
		}
	}
	switch (key) {
		case GLFW_KEY_F11: toggle_fullscreen(w); return;
		case GLFW_KEY_ESCAPE: if (g_is_fullscreen) toggle_fullscreen(w); return;
		case GLFW_KEY_A: Global::ItDiv = std::max(8.0_sr, Global::ItDiv / 2); Global::Redraw = true; break;
		case GLFW_KEY_S: Global::ItDiv = std::min(0x1p48_sr, Global::ItDiv * 2); Global::Redraw = true; break;
		case GLFW_KEY_E: Global::ColorCyclingSpeed = -0.1; Global::Redraw = true; break;
		case GLFW_KEY_R: Global::ColorCyclingSpeed =  0.1; Global::Redraw = true; break;
		case GLFW_KEY_D: Global::ItLim = std::min(UINT64_C(1) << 48, Global::ItLim * 2); FContext.ParameterChanged = true; break;
		case GLFW_KEY_F: Global::ItLim = std::max(UINT64_C(8), Global::ItLim / 2);       FContext.ParameterChanged = true; break;
		default: break;
	}
}

// argv[i] (UTF-8) -> wchar_t** for ParseCLI.
static std::vector<std::wstring> g_argv_storage;
static std::vector<wchar_t *> g_argv_ptrs;

static void build_wide_argv(int argc, char **argv) {
	g_argv_storage.clear();
	g_argv_ptrs.clear();
	g_argv_storage.reserve(argc);
	g_argv_ptrs.reserve(argc + 1);
	std::mbstate_t state{};
	for (int i = 0; i < argc; i++) {
		const char *s = argv[i];
		std::wstring out;
		while (*s) {
			wchar_t wc;
			size_t consumed = std::mbrtowc(&wc, s, MB_CUR_MAX, &state);
			if (consumed == size_t(-1) || consumed == size_t(-2)) {
				wc = (unsigned char)*s; consumed = 1; state = std::mbstate_t{};
			} else if (consumed == 0) {
				break;
			}
			out.push_back(wc);
			s += consumed;
		}
		g_argv_storage.push_back(std::move(out));
	}
	for (auto &s : g_argv_storage) g_argv_ptrs.push_back(s.data());
	g_argv_ptrs.push_back(nullptr);
}

int main(int argc, char **argv) {
	std::setlocale(LC_ALL, "");
	// CLI number parsing uses wcstod -- force C locale for decimal point so
	// "--zoom-step 0.97" works regardless of system locale (e.g. de_DE uses ',').
	std::setlocale(LC_NUMERIC, "C");

	build_wide_argv(argc, argv);
	ParseCLI((int)g_argv_storage.size(), g_argv_ptrs.data());
	if (!g_cli.valid) {
		if (g_cli.error == L"help") {
			ShowHelp();
			return 0;
		}
		std::fwprintf(stderr, L"Imagina: %ls\n", g_cli.error.c_str());
		return 1;
	}

	glfwSetErrorCallback(glfw_error_cb);
	if (!glfwInit()) {
		std::fprintf(stderr, "glfwInit failed\n");
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

	g_glfw_window = glfwCreateWindow((int)InitialWindowWidth, (int)InitialWindowHeight,
		"Imagina (Linux)", nullptr, nullptr);
	if (!g_glfw_window) {
		std::fprintf(stderr, "glfwCreateWindow failed\n");
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(g_glfw_window);
	glfwSwapInterval(1);

	int fb_w, fb_h;
	glfwGetFramebufferSize(g_glfw_window, &fb_w, &fb_h);
	WindowWidth  = (size_t)fb_w;
	WindowHeight = (size_t)fb_h;
	Global::SizeChanged = true;

	glfwSetFramebufferSizeCallback(g_glfw_window, framebuffer_size_cb);
	glfwSetCursorPosCallback(g_glfw_window, cursor_pos_cb);
	glfwSetMouseButtonCallback(g_glfw_window, mouse_button_cb);
	glfwSetScrollCallback(g_glfw_window, scroll_cb);
	glfwSetKeyCallback(g_glfw_window, key_cb);

	// ImGui setup.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(g_glfw_window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	InitRenderResources();
	Computation::Init();

	FContext.CurrentLocation = { 0.0, 0.0, 2.0 };
	FContext.RenderLocation  = { 0.0, 0.0, 2.0 };
	FContext.EvalLocation    = { 0.0, 0.0, 2.0 };
	FContext.ImageWidth      = InitialWindowWidth;
	FContext.ImageHeight     = InitialWindowHeight;

	Global::Initialized = true;

	ApplyCLISettings();

	while (!glfwWindowShouldClose(g_glfw_window)) {
		glfwPollEvents();

		FContext.Update();

		BeginRender();
		RenderFractal(FContext);

		Global::Redraw = false;
		if (Global::ColorCyclingSpeed != 0.0) Global::Redraw = true;

		// ImGui frame on top of the fractal.
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		draw_menu_bar();
		draw_iter_dialog();
		draw_about();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		EndRender();

		UpdateWindowTitle();
		CheckAutoRenderComplete();
		CheckFrameSequenceProgress();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	FContext.pixelManager.Abort();
	while (!FContext.pixelManager.Completed()) {}
	if (FContext.ReferenceWorker.joinable()) FContext.ReferenceWorker.detach();

	glfwDestroyWindow(g_glfw_window);
	glfwTerminate();
	return 0;
}
