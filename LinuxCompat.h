// Centralized Linux-side translations of common MSVC/Win32 idioms.
// Included from Includes.h when IMAGINA_LINUX is defined.

#pragma once

#ifdef IMAGINA_LINUX

#include <sys/sysinfo.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

// MSVC's force-inline -> GCC/clang equivalent.
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

// MSVC's scanf_s -> POSIX sscanf for the parse-N-numbers usage in Imagina.
#ifndef sscanf_s
#define sscanf_s sscanf
#endif

// wchar_t* -> UTF-8 std::string (using the C locale).
inline std::string imagina_wchar_to_utf8(const wchar_t *s) {
	if (!s) return std::string();
	std::mbstate_t state{};
	const wchar_t *src = s;
	size_t len = std::wcsrtombs(nullptr, &src, 0, &state);
	if (len == size_t(-1)) return std::string();
	std::string out(len, '\0');
	src = s;
	state = std::mbstate_t{};
	std::wcsrtombs(out.data(), &src, len, &state);
	return out;
}

// Stub-in for Windows GUI MessageBox: just log to stderr.
#ifndef MB_OK
#define MB_OK 0
#define MB_ICONERROR 0
#define MB_ICONINFORMATION 0
#define MB_TASKMODAL 0
#endif

inline int imagina_message_box(void * /*hwnd*/, const wchar_t *msg, const wchar_t *title, unsigned int /*flags*/) {
	std::fprintf(stderr, "[%ls] %ls\n", title ? title : L"Imagina", msg ? msg : L"");
	return 0;
}
#define MessageBoxW(hwnd, msg, title, flags) imagina_message_box((hwnd), (msg), (title), (flags))

// _wfopen_s -> fopen with UTF-8 path. Sets *fpp, returns 0 on success.
inline int imagina_wfopen_s(FILE **fpp, const wchar_t *path, const wchar_t *mode) {
	if (!fpp) return 22;
	std::string p = imagina_wchar_to_utf8(path);
	std::string m = imagina_wchar_to_utf8(mode);
	*fpp = std::fopen(p.c_str(), m.c_str());
	return *fpp ? 0 : 1;
}
#define _wfopen_s imagina_wfopen_s

#endif // IMAGINA_LINUX
