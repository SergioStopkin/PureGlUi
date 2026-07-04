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
)

# Resolve the ui/ prefix from the include/ root. UI framework tests stay light - pure C++, no GL/X11 - matching the fw's
# own dependency-free surface.
target_include_directories(${TARGET} PRIVATE ${CMAKE_HOME_DIRECTORY}/include)

# Fixture directory (TestIconStore loads test/data/iconstore.json), as an absolute path so it resolves regardless of the
# run directory.
target_compile_definitions(${TARGET} PRIVATE TEST_DATA_DIR="${CMAKE_HOME_DIRECTORY}/test/data")

target_link_libraries(${TARGET} GTest::gtest GTest::gtest_main pthread)
