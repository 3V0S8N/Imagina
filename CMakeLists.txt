cmake_minimum_required (VERSION 3.16)

project ("Imagina")

option(ENABLE_CUSTOM_FORMULA "Enable custom formula" OFF)
option(IMAGINA_LINUX_PORT "Build the Linux port (GLFW + ImGui + GMP)" OFF)
option(MPIR_USE_DLL "Use dll version of MPIR (Windows only)" ON)
set(INCLUDE_PATH "include" CACHE STRING "Include path")
set(LIBRARY_PATH "lib" CACHE STRING "Library path")

if(NOT WIN32)
	set(IMAGINA_LINUX_PORT ON CACHE BOOL "Force Linux port off Windows" FORCE)
endif()

FILE(GLOB SourceFiles "*.cpp")

if(IMAGINA_LINUX_PORT)
	# Exclude Windows-only entrypoints. Replacements live in main_linux.cpp / LinuxWindow.cpp.
	list(FILTER SourceFiles EXCLUDE REGEX ".*MainWindow\\.cpp$")
	list(FILTER SourceFiles EXCLUDE REGEX ".*/main\\.cpp$")
	list(FILTER SourceFiles EXCLUDE REGEX "^main\\.cpp$")
	# Heavily Win32-coupled files. Phase 1: skip. Re-introduced with platform abstraction in later phases.
	#   CLI.cpp: HWND, MessageBox, PostQuitMessage
	#   Overlay.cpp: GDI text rendering (-> FreeType later)
	#   File.cpp: wchar_t fopen/ifstream + MessageBoxW dialogs
	#   GLUtils.cpp: wglGetProcAddress (uses glXGetProcAddress on Linux)
	#   Render.cpp: SwapBuffers + heavy GL state setup
	#   PixelManager.cpp: GL texture pipeline
	foreach(skip)
		list(FILTER SourceFiles EXCLUDE REGEX ".*/${skip}\\.cpp$")
		list(FILTER SourceFiles EXCLUDE REGEX "^${skip}\\.cpp$")
	endforeach()
endif()

if(ENABLE_CUSTOM_FORMULA)
	FILE(GLOB SourceFilesEncoder "Encoder/*.cpp")
else()
	set(SourceFilesEncoder "")
endif()

if(IMAGINA_LINUX_PORT)
	# Vendored single-file file-dialog lib (zlib license).
	set(TFD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyfiledialogs.c")
	# Vendored Dear ImGui (MIT license).
	set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
	set(IMGUI_SOURCES
		"${IMGUI_DIR}/imgui.cpp"
		"${IMGUI_DIR}/imgui_draw.cpp"
		"${IMGUI_DIR}/imgui_widgets.cpp"
		"${IMGUI_DIR}/imgui_tables.cpp"
		"${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
		"${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")
	add_executable (Imagina ${SourceFiles} ${SourceFilesEncoder} ${TFD_SOURCES} ${IMGUI_SOURCES})
	target_include_directories(Imagina PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/third_party"
		"${IMGUI_DIR}"
		"${IMGUI_DIR}/backends")
	set_source_files_properties("${TFD_SOURCES}" ${IMGUI_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
else()
	add_executable (Imagina WIN32 ${SourceFiles} ${SourceFilesEncoder} "Resource.rc")
	set_source_files_properties("Resource.rc" PROPERTIES LANGUAGE RC)
endif()

target_precompile_headers (Imagina PRIVATE "Includes.h")

target_compile_definitions(Imagina PRIVATE UNICODE)

if(IMAGINA_LINUX_PORT)
	target_compile_definitions(Imagina PRIVATE IMAGINA_LINUX)
	# Imagina was written against MSVC. GCC compat flags:
	target_compile_options(Imagina PRIVATE -fms-extensions -fpermissive -Wno-permissive)
	# -fdeclspec only exists on clang, not gcc:
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		target_compile_options(Imagina PRIVATE -fdeclspec)
	endif()

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
	pkg_check_modules(GMP REQUIRED IMPORTED_TARGET gmp gmpxx)
	pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
	pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
	find_package(OpenGL REQUIRED)

	target_link_libraries(Imagina
		PkgConfig::GLFW
		PkgConfig::GMP
		PkgConfig::PNG
		PkgConfig::FREETYPE
		OpenGL::GL
		pthread
		dl)

	if(ENABLE_CUSTOM_FORMULA)
		pkg_check_modules(MPFR REQUIRED IMPORTED_TARGET mpfr)
		pkg_check_modules(MPC  REQUIRED IMPORTED_TARGET mpc)
		target_link_libraries(Imagina PkgConfig::MPFR PkgConfig::MPC symengine)
		target_compile_definitions(Imagina PRIVATE ENABLE_CUSTOM_FORMULA)
	endif()
else()
	target_include_directories(Imagina SYSTEM PRIVATE ${INCLUDE_PATH})
	target_include_directories(Imagina SYSTEM PRIVATE "${INCLUDE_PATH}/GL")
	target_link_directories(Imagina PRIVATE ${LIBRARY_PATH})
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_include_directories(Imagina SYSTEM PRIVATE "${INCLUDE_PATH}/Debug")
		target_link_directories(Imagina PRIVATE "${LIBRARY_PATH}/Debug")
	else()
		target_include_directories(Imagina SYSTEM PRIVATE "${INCLUDE_PATH}/Release")
		target_link_directories(Imagina PRIVATE "${LIBRARY_PATH}/Release")
	endif()

	target_link_libraries(Imagina "opengl32")
	target_link_libraries(Imagina "comctl32")
	target_link_libraries(Imagina "png16")
	target_link_libraries(Imagina "z")
	target_link_libraries(Imagina "dwmapi")
	target_link_libraries(Imagina "mpirxx")
	target_link_libraries(Imagina "mpir")
	target_link_options(Imagina PRIVATE "-municode")

	if(MPIR_USE_DLL)
		target_compile_definitions(Imagina PRIVATE MSC_USE_DLL)
	else()
		target_link_libraries(Imagina "mpirxx")
	endif()

	if(ENABLE_CUSTOM_FORMULA)
		target_link_libraries(Imagina "mpfr")
		target_link_libraries(Imagina "mpc")
		target_link_libraries(Imagina "symengine")
		target_compile_definitions(Imagina PRIVATE ENABLE_CUSTOM_FORMULA)
	endif()
endif()

set_property(TARGET Imagina PROPERTY CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-math-errno")

set_source_files_properties(HInfLAEvaluator.cpp PROPERTIES COMPILE_FLAGS "-fassociative-math -ffp-contract=fast" SKIP_PRECOMPILE_HEADERS ON)
