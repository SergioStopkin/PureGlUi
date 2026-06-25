set(TARGET component-tests)

add_executable(${TARGET} TestPopupCornerCapture.cpp TestSvgIntrinsic.cpp TestFontBaseline.cpp TestDockColumn.cpp)

# Full graphics stack (GL/X11/EGL/cairo/rsvg/freetype + HAVE_X11/HAVE_EGL defs) comes from the ui INTERFACE target; only
# gtest is test-specific.
target_link_libraries(${TARGET} ui GTest::gtest GTest::gtest_main)

# Path to res/ directory for loading theme/layout in tests (TestDockColumn bootstraps a real ResManager)
target_compile_definitions(${TARGET} PRIVATE TEST_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")
target_compile_definitions(${TARGET} PRIVATE PROJECT_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")
