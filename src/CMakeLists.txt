# See TARGET_SRC in .cicd-config
add_executable(${TARGET_SRC} main.cpp sig.cpp)

# macOS: compile as Objective-C++ so Cocoa headers work
if(APPLE)
    set_source_files_properties(main.cpp sig.cpp PROPERTIES LANGUAGE OBJCXX)
    target_compile_definitions(${TARGET_SRC} PRIVATE GL_SILENCE_DEPRECATION)
endif()

# MSVC: force-include <windows.h> first so the win32 windowing backend sees a clean macro environment. /FS serializes
# PDB writes (some shared filesystems break PDB locking).
if(MSVC)
    target_compile_options(${TARGET_SRC} PRIVATE /FI"windows.h" /FS)
    # Hide console window in release builds; dev builds keep it for stdout/stderr
    if(NOT DEV_BUILD)
        target_link_options(${TARGET_SRC} PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()
endif()

# Enable warnings for our code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${TARGET_SRC} PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(
    ${TARGET_SRC} PRIVATE OpenGL::GL Freetype::Freetype ${FONTCONFIG_LIBRARIES} ${GLEW_LIBRARY} ${LIBRSVG_LIBRARIES}
                          ${CAIRO_LIBRARIES}
)

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(${TARGET_SRC} PRIVATE user32 gdi32 opengl32 ws2_32)
else()
    target_link_libraries(${TARGET_SRC} PRIVATE pthread dl)
endif()

# EGL (Linux/macOS GL context creation via EglContext)
if(EGL_LIBRARY)
    target_link_libraries(${TARGET_SRC} PRIVATE ${EGL_LIBRARY})
endif()

# Link jemalloc if detected (optional allocator replacement).
if(USE_JEMALLOC AND JEMALLOC_FOUND)
    target_link_libraries(${TARGET_SRC} PRIVATE ${JEMALLOC_LIBRARIES})
endif()

if(APPLE)
    target_link_libraries(
        ${TARGET_SRC} PRIVATE "-framework Foundation" "-framework AppKit" "-framework QuartzCore"
                              "-framework UniformTypeIdentifiers"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TARGET_SRC} PRIVATE ${X11_LIBRARIES} ${XCOMPOSITE_LIBRARY} ${XRENDER_LIBRARY})

    # Add Wayland libraries when building with Wayland support
    if(HAVE_WAYLAND)
        target_link_libraries(${TARGET_SRC} PRIVATE ${WAYLAND_LIBRARIES} ${XKB_LIBRARIES} ${WAYLAND_PROTOCOL_LIB})
        target_include_directories(${TARGET_SRC} PRIVATE ${CMAKE_BINARY_DIR})
    endif()
endif()

# Copy resources to build directory
add_custom_command(
    TARGET ${TARGET_SRC}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${TARGET_SRC}>/res
    COMMENT "Copying resource files to build directory"
)
