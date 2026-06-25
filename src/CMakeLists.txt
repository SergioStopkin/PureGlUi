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

# Framework usage requirements (GL/X11/EGL/cairo/rsvg/freetype/wayland + their includes + C++20) all come from the ui
# INTERFACE target. jemalloc is an app-level allocator choice, so it stays here rather than in the fw contract.
target_link_libraries(${TARGET_SRC} PRIVATE ui)

if(USE_JEMALLOC AND JEMALLOC_FOUND)
    target_link_libraries(${TARGET_SRC} PRIVATE ${JEMALLOC_LIBRARIES})
endif()

# Copy resources to build directory
add_custom_command(
    TARGET ${TARGET_SRC}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${TARGET_SRC}>/res
    COMMENT "Copying resource files to build directory"
)
