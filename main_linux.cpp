// Linux entrypoint stub. Phase 1: just exercises the build chain and reports
// whether the headless CLI can parse arguments. No actual rendering yet --
// PixelManager needs an OpenGL context which will arrive with the GLFW port.

#include "Includes.h"
#include "CLI.h"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <vector>
#include <string>

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
		size_t len = std::strlen(s);
		std::wstring out;
		out.reserve(len);
		while (*s) {
			wchar_t wc;
			const char *src = s;
			size_t consumed = std::mbrtowc(&wc, src, MB_CUR_MAX, &state);
			if (consumed == size_t(-1) || consumed == size_t(-2)) {
				wc = (unsigned char)*s;
				consumed = 1;
				state = std::mbstate_t{};
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

	std::fprintf(stderr,
		"Imagina Linux port -- Phase 1 build stub.\n"
		"CLI parsed OK. Headless rendering not wired yet.\n"
		"(GLFW window + GL context coming in next phase.)\n");
	return 0;
}
