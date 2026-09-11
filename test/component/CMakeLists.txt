set(TARGET component-tests)

add_executable(
    ${TARGET}
    TestPopupCornerCapture.cpp
    TestSvgIntrinsic.cpp
    TestFontBaseline.cpp
    TestDockColumn.cpp
    TestDockRowLayout.cpp
    TestDockScrollbar.cpp
    TestDockSlider.cpp
    TestIconFill.cpp
    TestWindowGeometry.cpp
    TestGlSmoke.cpp
    TestUiDriver.cpp
    TestChromeLifecycle.cpp
    TestContentSurface.cpp
    TestInitHooks.cpp
)

# macOS: any TU that reaches Cocoa must compile as Objective-C++ (same as the app's src/main.cpp). Three do: TestGlSmoke
# creates a real window via NativeWindow -> macoswindow.h, TestInitHooks includes ui/shell.h, which pulls Cocoa
# transitively through action/openfile.h -> io/filedialog.h, and TestWindowGeometry includes ui/window/windowmanager.h,
# which pulls it through eventfactory.h -> macosevent.h. So on macOS, including shell.h or windowmanager.h at all forces
# Obj-C++ on the includer - windowless says nothing about it. TestContentSurface includes windowmanager.h too but is #if
# defined(HAVE_X11), so it is an empty TU here. The rest are pure C++. GL_SILENCE_DEPRECATION quiets the deprecated-GL
# SDK annotations for these Obj-C++ units (CMAKE_CXX_FLAGS' -Wno-... only covers CXX).
if(APPLE)
    set_source_files_properties(TestGlSmoke.cpp TestInitHooks.cpp TestWindowGeometry.cpp PROPERTIES LANGUAGE OBJCXX)
    target_compile_definitions(${TARGET} PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Full graphics stack (GL/X11/EGL/cairo/rsvg/freetype + HAVE_X11/HAVE_EGL defs) comes from the ui INTERFACE target; only
# gtest is test-specific. GTest::gtest_main pulls GTest::gtest transitively; listing gtest too puts libgtest.a on the
# link line twice (macOS ld warns on duplicate libraries).
target_link_libraries(${TARGET} ui GTest::gtest_main)

# Path to res/ directory for loading theme/layout in tests (TestDockColumn bootstraps a real ResManager)
target_compile_definitions(${TARGET} PRIVATE TEST_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")
target_compile_definitions(${TARGET} PRIVATE PROJECT_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")
