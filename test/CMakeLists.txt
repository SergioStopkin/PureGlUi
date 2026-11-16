find_package(GTest REQUIRED)
include(GoogleTest)

# clang 21 warns in gtest's own char8_t printer (gtest-printers.h), once per test that includes it; gtest is a third
# party's, and not every compiler takes its install dir for a system one. Only the tests, so our code keeps the warning
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wcharacter-conversion HAS_CHARACTER_CONVERSION)
if(HAS_CHARACTER_CONVERSION)
    add_compile_options(-Wno-character-conversion)
endif()

add_subdirectory(unit)
add_subdirectory(component)
