// Centralized Linux-side translations of common MSVC/Win32 idioms.
// Included from Includes.h when IMAGINA_LINUX is defined.

#pragma once

#ifdef IMAGINA_LINUX

#include <sys/sysinfo.h>
#include <cstring>

// MSVC's force-inline -> GCC/clang equivalent.
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

// MSVC's scanf_s -> POSIX sscanf for the parse-N-numbers usage in Imagina.
#ifndef sscanf_s
#define sscanf_s sscanf
#endif

#endif // IMAGINA_LINUX
