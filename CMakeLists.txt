cmake_minimum_required(VERSION 3.12)
message(STATUS "CMake version: ${CMAKE_VERSION}")

# Set the project name to the current directory name (see .cicd-config)
project(
    ${PROJECT_NAME}
    VERSION 0.1.0
    LANGUAGES C CXX
)

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

# --- GLEW (OpenGL extension loader) ---
# macOS provides GL functions directly via framework - no extension loader needed.
if(WIN32)
    # On Windows (vcpkg), use find_package for GLEW
    find_package(GLEW CONFIG REQUIRED)
    set(GLEW_INCLUDE_DIR "")
    set(GLEW_LIBRARY GLEW::GLEW)
elseif(NOT APPLE)
    find_path(GLEW_INCLUDE_DIR GL/glew.h
        /usr/include
        /usr/local/include
        /usr/include/GL
        /usr/local/include/GL
    )
    find_library(GLEW_LIBRARY NAMES GLEW glew32 glew glew32s
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/local/lib64
    )
    message(STATUS "GLEW_INCLUDE_DIR: ${GLEW_INCLUDE_DIR}")
    message(STATUS "GLEW_LIBRARY: ${GLEW_LIBRARY}")
    if(NOT GLEW_INCLUDE_DIR OR NOT GLEW_LIBRARY)
        message(FATAL_ERROR "GLEW not found. Please install libglew-dev.")
    endif()
    include_directories(${GLEW_INCLUDE_DIR})
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SUPPRESS_REGENERATION ON)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

# Dependencies
if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
endif()
find_package(OpenGL REQUIRED)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
endif()

# Try to locate XComposite and XRender libraries for ARGB compositing support (Linux only)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(XCOMPOSITE_LIBRARY NAMES Xcomposite libXcomposite PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib64 NO_DEFAULT_PATH)
    find_library(XRENDER_LIBRARY NAMES Xrender libXrender PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib64 NO_DEFAULT_PATH)
    if(XCOMPOSITE_LIBRARY)
        message(STATUS "Found Xcomposite library: ${XCOMPOSITE_LIBRARY}")
    else()
        message(STATUS "Xcomposite library not found; ARGB compositing may be unavailable.")
    endif()
    if(XRENDER_LIBRARY)
        message(STATUS "Found Xrender library: ${XRENDER_LIBRARY}")
    else()
        message(STATUS "Xrender library not found; ARGB compositing may be unavailable.")
    endif()
endif()
find_package(Freetype REQUIRED)
if(WIN32)
    find_package(Fontconfig REQUIRED)
    set(FONTCONFIG_LIBRARIES Fontconfig::Fontconfig)
else()
    pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
    set(FONTCONFIG_LIBRARIES PkgConfig::FONTCONFIG)
endif()

# 3rd party headers: nlohmann/json (header-only, vendored under 3rd/include).
set(3RD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rd/include")

add_compile_definitions(APP_VERSION="${PROJECT_VERSION}")

# Include directories. Own headers live under include/ (common/ + ui/); a
# "ui/..." / "common/..." / "sig.h" include resolves against this one root.
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIR}
)

# Ensure Freetype headers are available to all targets (e.g., tests)
# Prefer the variable populated by find_package(Freetype), fall back to common system path
if(DEFINED FREETYPE_INCLUDE_DIRS)
    include_directories(${FREETYPE_INCLUDE_DIRS})
else()
    include_directories(/usr/include/freetype2)
endif()

# Mark 3rd party (nlohmann) includes as SYSTEM to suppress their warnings
include_directories(SYSTEM ${3RD_INCLUDE_DIR})

if(ASAN)
    if(MSVC)
        message(WARNING "ASAN not supported on MSVC. Use a GNU/Linux dev build for ASAN.")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libasan")
        endif()
    endif()
endif()

option(ENABLE_CLANG_FORMATTER "Use clang-formatter to format source code?" ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OPENGL_gl_LIBRARY "/usr/lib/x86_64-linux-gnu/libGL.so" CACHE FILEPATH "OpenGL library")
    set(OPENGL_INCLUDE_DIR "/usr/include/GL" CACHE PATH "OpenGL include directory")
endif()

# EGL detection (Linux only - Windows uses WGL, macOS uses NSOpenGL)
if(NOT WIN32)
    find_path(EGL_INCLUDE_DIR EGL/egl.h
        PATHS /usr/include /usr/local/include
    )
    find_library(EGL_LIBRARY NAMES EGL
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib64
    )
    if(EGL_INCLUDE_DIR AND EGL_LIBRARY)
        message(STATUS "Found EGL: ${EGL_LIBRARY}")
        message(STATUS "EGL include: ${EGL_INCLUDE_DIR}")
        set(EGL_FOUND TRUE)
        include_directories(${EGL_INCLUDE_DIR})
    else()
        message(STATUS "EGL not found - EGL support will be disabled")
        set(EGL_FOUND FALSE)
    endif()
else()
    set(EGL_FOUND FALSE)
endif()

# ===========================================================================
# Wayland detection (optional, for native Wayland support on Linux)
# ===========================================================================
option(WITH_WAYLAND "Build with native Wayland support" ON)

if(WITH_WAYLAND AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(WAYLAND QUIET wayland-client wayland-egl wayland-cursor)
    pkg_check_modules(XKB QUIET xkbcommon)
    pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)

    if(WAYLAND_FOUND AND XKB_FOUND)
        message(STATUS "Found Wayland: ${WAYLAND_LIBRARIES}")
        message(STATUS "Found XKB: ${XKB_LIBRARIES}")
        set(HAVE_WAYLAND TRUE)
        add_compile_definitions(HAVE_WAYLAND=1)

        # Find wayland-scanner for protocol header generation
        find_program(WAYLAND_SCANNER wayland-scanner)
        if(WAYLAND_SCANNER AND WAYLAND_PROTOCOLS_FOUND)
            pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
            set(XDG_SHELL_PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")

            if(EXISTS "${XDG_SHELL_PROTOCOL}")
                message(STATUS "Found xdg-shell protocol: ${XDG_SHELL_PROTOCOL}")

                # Generate xdg-shell client header
                add_custom_command(
                    OUTPUT ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h
                    COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_PROTOCOL}
                            ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h
                    DEPENDS ${XDG_SHELL_PROTOCOL}
                    COMMENT "Generating xdg-shell client header"
                )

                # Generate xdg-shell protocol code
                add_custom_command(
                    OUTPUT ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c
                    COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_PROTOCOL}
                            ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c
                    DEPENDS ${XDG_SHELL_PROTOCOL}
                    COMMENT "Generating xdg-shell protocol code"
                )

                # Create a library target for the generated protocol code
                add_library(wayland-protocols-gen STATIC
                    ${CMAKE_BINARY_DIR}/xdg-shell-protocol.c
                    ${CMAKE_BINARY_DIR}/xdg-shell-client-protocol.h
                )
                set_target_properties(wayland-protocols-gen PROPERTIES LINKER_LANGUAGE C)
                target_include_directories(wayland-protocols-gen PUBLIC ${CMAKE_BINARY_DIR})
                target_link_libraries(wayland-protocols-gen PUBLIC ${WAYLAND_LIBRARIES})
                set(WAYLAND_PROTOCOL_LIB wayland-protocols-gen)
            else()
                message(WARNING "xdg-shell protocol XML not found at ${XDG_SHELL_PROTOCOL}")
                set(HAVE_WAYLAND FALSE)
            endif()
        else()
            message(WARNING "wayland-scanner or wayland-protocols not found; Wayland support disabled")
            set(HAVE_WAYLAND FALSE)
        endif()
    else()
        message(STATUS "Wayland libraries not found; Wayland support disabled")
        set(HAVE_WAYLAND FALSE)
    endif()
else()
    set(HAVE_WAYLAND FALSE)
endif()

# X11 is always available on Linux (already detected above)
if(X11_FOUND)
    add_compile_definitions(HAVE_X11=1)
endif()

# ===========================================================================
# librsvg + Cairo - SVG rendering
# ===========================================================================
if(WIN32)
    find_library(CAIRO_LIBRARY NAMES cairo)
    find_path(CAIRO_INCLUDE_DIR cairo/cairo.h)
    find_library(LIBRSVG_LIBRARY NAMES rsvg-2 librsvg-2)
    find_path(LIBRSVG_INCLUDE_DIR librsvg/rsvg.h PATH_SUFFIXES librsvg-2.0)
    if(NOT CAIRO_LIBRARY OR NOT CAIRO_INCLUDE_DIR)
        message(FATAL_ERROR "Cairo not found. Install via: vcpkg install cairo --triplet x64-windows-release")
    endif()
    if(NOT LIBRSVG_LIBRARY OR NOT LIBRSVG_INCLUDE_DIR)
        message(FATAL_ERROR "librsvg not found. Install via: vcpkg install librsvg --triplet x64-windows-release")
    endif()
    # Add all vcpkg include subdirectories (librsvg transitively depends on glib, gdk-pixbuf, pango, etc.)
    set(_VCPKG_INC "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    set(_VCPKG_LIB "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
    include_directories(
        ${_VCPKG_INC}/cairo
        ${_VCPKG_INC}/glib-2.0
        ${_VCPKG_LIB}/glib-2.0/include
        ${_VCPKG_INC}/gdk-pixbuf-2.0
        ${_VCPKG_INC}/librsvg-2.0
        ${_VCPKG_INC}/libcroco-0.6
        ${_VCPKG_INC}/pango-1.0
        ${_VCPKG_INC}/libxml2
        ${_VCPKG_INC}/pixman-1
        ${_VCPKG_INC}/fribidi
        ${_VCPKG_INC}/harfbuzz
    )
    find_library(GLIB_LIBRARY NAMES glib-2.0)
    find_library(GOBJECT_LIBRARY NAMES gobject-2.0)
    find_library(GIO_LIBRARY NAMES gio-2.0)
    set(CAIRO_LIBRARIES ${CAIRO_LIBRARY})
    set(LIBRSVG_LIBRARIES ${LIBRSVG_LIBRARY} ${GOBJECT_LIBRARY} ${GLIB_LIBRARY} ${GIO_LIBRARY})
else()
    pkg_check_modules(LIBRSVG REQUIRED IMPORTED_TARGET librsvg-2.0)
    pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
    set(LIBRSVG_LIBRARIES PkgConfig::LIBRSVG)
    set(CAIRO_LIBRARIES PkgConfig::CAIRO)
endif()

# ===========================================================================
# jemalloc - optional drop-in replacement for the system allocator. Gracefully
# absent when not installed; off the critical path for the framework.
# ===========================================================================
option(USE_JEMALLOC "Link against jemalloc to replace the system allocator" ON)
set(JEMALLOC_FOUND FALSE)
set(JEMALLOC_LIBRARIES "")
if(USE_JEMALLOC)
    if(WIN32)
        find_library(JEMALLOC_LIBRARY NAMES jemalloc)
        find_path(JEMALLOC_INCLUDE_DIR jemalloc/jemalloc.h)
        if(JEMALLOC_LIBRARY AND JEMALLOC_INCLUDE_DIR)
            set(JEMALLOC_FOUND TRUE)
            set(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
            include_directories(${JEMALLOC_INCLUDE_DIR})
            message(STATUS "Found jemalloc: ${JEMALLOC_LIBRARY}")
        endif()
    else()
        pkg_check_modules(JEMALLOC IMPORTED_TARGET jemalloc)
        if(JEMALLOC_FOUND)
            set(JEMALLOC_LIBRARIES PkgConfig::JEMALLOC)
            message(STATUS "Found jemalloc: ${JEMALLOC_LIBRARIES}")
        endif()
    endif()
    if(NOT JEMALLOC_FOUND)
        message(STATUS "jemalloc not found; falling back to system allocator")
    endif()
endif()

add_subdirectory(src)

# Tests are opt-in. gtest is a test-only dependency, so a demo-only build
# (BUILD_TESTING=OFF) needs no gtest; find_package(GTest) lives in test/.
option(BUILD_TESTING "Build the unit tests" ON)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
