set(TARGET component-tests)

add_executable(
    ${TARGET}
    TestPopupCornerCapture.cpp
    TestSvgIntrinsic.cpp
    TestFontBaseline.cpp
    TestDockColumn.cpp
    TestGlSmoke.cpp
    TestUiDriver.cpp
    TestChromeLifecycle.cpp
    TestContentSurface.cpp
    TestInitHooks.cpp
)

# macOS: TestGlSmoke creates a real window via NativeWindow -> Cocoa (macoswindow.h), so it must compile as
# Objective-C++ (same as the app's src/main.cpp). The other suites are windowless pure C++. GL_SILENCE_DEPRECATION
# quiets the deprecated-GL SDK annotations for this Obj-C++ unit (CMAKE_CXX_FLAGS' -Wno-... only covers CXX).
if(APPLE)
    set_source_files_properties(TestGlSmoke.cpp PROPERTIES LANGUAGE OBJCXX)
    target_compile_definitions(${TARGET} PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Full graphics stack (GL/X11/EGL/cairo/rsvg/freetype + HAVE_X11/HAVE_EGL defs) comes from the ui INTERFACE target; only
# gtest is test-specific. GTest::gtest_main pulls GTest::gtest transitively; listing gtest too puts libgtest.a on the
# link line twice (macOS ld warns on duplicate libraries).
target_link_libraries(${TARGET} ui GTest::gtest_main)

# Path to res/ directory for loading theme/layout in tests (TestDockColumn bootstraps a real ResManager)
target_compile_definitions(${TARGET} PRIVATE TEST_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")
target_compile_definitions(${TARGET} PRIVATE PROJECT_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")
