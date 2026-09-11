set(TARGET unit-tests)

add_executable(
    ${TARGET}
    TestRegistry.cpp
    TestConfig.cpp
    TestTabBar.cpp
    TestRenderInterfaces.cpp
    TestIntent.cpp
    TestMenuDisable.cpp
    TestBackgroundWorker.cpp
    TestColor.cpp
    TestSanitize.cpp
    TestTextAlign.cpp
    TestClickCounter.cpp
    TestIconStore.cpp
    TestMenuDepthCap.cpp
    TestMenuSanitize.cpp
    TestNonCopyable.cpp
    TestSession.cpp
    TestUtil.cpp
    TestResKeys.cpp
    TestConvert.cpp
    TestLayoutStore.cpp
    TestThemeStore.cpp
    TestBit.cpp
    TestUnicode.cpp
    TestResPath.cpp
    TestLocaleManager.cpp
    TestShortcutStore.cpp
    TestDialogStore.cpp
    TestDockStore.cpp
    TestJson.cpp
    TestFs.cpp
    TestResManager.cpp
    TestMenuStore.cpp
    TestMenuIconRole.cpp
    TestMinWindowSize.cpp
    TestScrollBar.cpp
    TestScrollbarRes.cpp
    TestSliderRes.cpp
    TestSlider.cpp
    TestTruncateText.cpp
    TestSig.cpp
)

# Resolve the ui/ prefix from the include/ root. UI framework tests stay light - pure C++, no GL/X11 - matching the fw's
# own dependency-free surface.
target_include_directories(${TARGET} PRIVATE ${CMAKE_HOME_DIRECTORY}/include)

# Fixture directory (TestIconStore loads test/data/iconstore.json), as an absolute path so it resolves regardless of the
# run directory.
target_compile_definitions(${TARGET} PRIVATE TEST_DATA_DIR="${CMAKE_HOME_DIRECTORY}/test/data")

# TestSession bootstraps a real ResManager (pure C++, no GL) to round-trip session.json.
target_compile_definitions(${TARGET} PRIVATE TEST_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")

# GTest::gtest_main pulls GTest::gtest transitively; listing gtest too puts libgtest.a on the link line twice (macOS ld
# warns on duplicate libraries).
target_link_libraries(${TARGET} GTest::gtest_main)

# pthread: gtest + std::thread need POSIX threads on Linux (libpthread); macOS resolves them via libSystem. MSVC has no
# pthread.lib and needs none (Windows threading is in the CRT), so link it only off Windows.
if(NOT WIN32)
    target_link_libraries(${TARGET} pthread)
endif()
