#ifndef _INCLUDES_H_
#define _INCLUDES_H_

#include "Configuration.h"
#ifdef IMAGINA_LINUX
#include "LinuxCompat.h"
#endif

#ifdef _WIN32
#define NOMINMAX
#if __has_include(<Windows.h>)
#include <Windows.h>
#else
#include <windows.h>
#endif

#define GLEW_STATIC
#else
#define GL_GLEXT_PROTOTYPES
#endif
#if __has_include(<GL/gl.h>)
#include <GL/gl.h>
#elif __has_include(<GLES3/gl3.h>)
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#error OpenGL Headers not found
#endif

#ifdef _WIN32
#include <glext.h>
#include <wglext.h>
#else
#if __has_include(<GL/glext.h>)
#include <GL/glext.h>
#endif
#endif

#ifdef GL_TEXTURE_RECTANGLE
#define USE_GL_TEXTURE_RECTANGLE
#endif

#ifdef USE_GL_TEXTURE_RECTANGLE
#define TARGET_TEXTRUE GL_TEXTURE_RECTANGLE
#else
#define TARGET_TEXTRUE GL_TEXTURE_2D
#endif

#define unused [[maybe_unused]]

#include <thread>
#include <stddef.h>
#include <stdint.h>
#include <chrono>
#include <iostream>
#include <glm/glm.hpp>

#include "PlatformDependent.h"
#include "Types.h"
#include "Global.h"
#include "Computation.h"
#include "FractalContext.h"
#include "Evaluator.h"
#include "HInfLAEvaluator.h"
#include "JitEvaluator.h"
#ifndef IMAGINA_LINUX
#include "MainWindow.h"
#endif
#include "Render.h"

inline uint32_t CountLeadingZeros(uint32_t n) {
#if defined(__GNUC__) || defined(__clang__)
	return __builtin_clz(n);
#elif defined(_WIN32)
	return _lzcnt_u32(n);
#else
#error
#endif
}

#define if_likely(...) if (__VA_ARGS__) [[likely]]
#define if_unlikely(...) if (__VA_ARGS__) [[unlikely]]

#endif
