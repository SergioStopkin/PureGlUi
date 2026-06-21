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
)

# Resolve the ui/ prefix from the include/ root. UI framework tests stay light - pure C++, no GL/X11 - matching the fw's
# own dependency-free surface.
target_include_directories(${TARGET} PRIVATE ${CMAKE_HOME_DIRECTORY}/include)

target_link_libraries(${TARGET} GTest::gtest GTest::gtest_main pthread)
