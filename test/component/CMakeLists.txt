set(TARGET component-tests)

add_executable(${TARGET} TestPopupCornerCapture.cpp)

# Framework headers (ui/ + common/) come from the global include path.
target_link_libraries(
    ${TARGET}
    GTest::gtest
    GTest::gtest_main
    pthread
    ${LIBRSVG_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${GLEW_LIBRARY}
    OpenGL::GL
    Freetype::Freetype
    ${FONTCONFIG_LIBRARIES}
    ${X11_LIBRARIES}
    dl
)

# Conditionally link EGL
if(EGL_FOUND)
    target_link_libraries(${TARGET} ${EGL_LIBRARY})
    target_compile_definitions(${TARGET} PRIVATE HAVE_EGL=1)
endif()

# Conditionally link Xcomposite and Xrender
if(XCOMPOSITE_LIBRARY)
    target_link_libraries(${TARGET} ${XCOMPOSITE_LIBRARY})
endif()
if(XRENDER_LIBRARY)
    target_link_libraries(${TARGET} ${XRENDER_LIBRARY})
endif()

# X11 compile definition
if(X11_FOUND)
    target_compile_definitions(${TARGET} PRIVATE HAVE_X11=1)
endif()

# Path to res/ directory for loading theme/layout in tests
target_compile_definitions(${TARGET} PRIVATE TEST_RES_DIR="${CMAKE_HOME_DIRECTORY}/res")
