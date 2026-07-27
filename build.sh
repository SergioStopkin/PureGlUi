#!/usr/bin/env bash
. .cicd-config

USAGE_TEXT="Usage: ./build.sh <dev|rel|test|ut|ct|cov>"

if [[ $# -eq 0 ]]; then
    PrintUsageAndExit
fi

CMAKE_ARGS=""
BUILD_ARGS=""

# Platform-specific compiler and build settings
if [[ "$OS" == "Windows_NT" ]]; then
    # MSVC: VS generator is multi-config, --config must be explicit at build time
    # /p:TrackFileAccess=false - disable MSBuild .tlog file tracking (breaks on some shared filesystems)
    BUILD_ARGS="--config Release -- /p:TrackFileAccess=false"
    export VCPKG_TARGET_TRIPLET=x64-windows-release
    export CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
    export MSYS_NO_PATHCONV=1
    CMAKE_ARGS="-DVCPKG_TARGET_TRIPLET=$VCPKG_TARGET_TRIPLET -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE"
else
    export CC=clang$CLANG_VERSION
    export CXX=clang++$CLANG_VERSION
fi

# Build type. gtest is a test-only dependency, so demo builds (dev/rel) disable
# BUILD_TESTING; the test builds enable it. ut/ct build one suite each so a
# compile failure names which suite; test builds both.
if [[ $1 == "dev" ]]; then
    BUILD_DIR=$BUILD_DIR_DEV
    CMAKE_ARGS="$CMAKE_ARGS -DASAN=ON -DBUILD_TESTING=OFF"
    if [[ "$OS" == "Windows_NT" ]]; then
        CMAKE_ARGS="$CMAKE_ARGS -DDEV_BUILD=ON"
    fi
    TARGET=$TARGET_SRC
elif [[ $1 == "rel" ]]; then
    BUILD_DIR=$BUILD_DIR_REL
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_TESTING=OFF"
    TARGET=$TARGET_SRC
elif [[ $1 == "test" ]]; then
    BUILD_DIR=$BUILD_DIR_REL
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_TESTING=ON"
    TARGET=$TARGET_TEST
elif [[ $1 == "ut" ]]; then
    BUILD_DIR=$BUILD_DIR_REL
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_TESTING=ON"
    TARGET=$TARGET_UT
elif [[ $1 == "ct" ]]; then
    BUILD_DIR=$BUILD_DIR_REL
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_TESTING=ON"
    TARGET=$TARGET_CT
elif [[ $1 == "cov" ]]; then
    BUILD_DIR=$BUILD_DIR_COV
    CMAKE_ARGS="$CMAKE_ARGS -DBUILD_TESTING=ON -DCOVERAGE=ON"
    TARGET=unit-tests
else
    PrintUsageAndExit
fi

CMAKE_ARGS="$CMAKE_ARGS -DTARGET_SRC=$TARGET_SRC -DWITH_WAYLAND=OFF"

# || exit: a failed configure must not fall through to building the stale cache
cmake -S $CMAKE_SOURCE_DIR -B $BUILD_DIR -DPROJECT_NAME=$PROJECT_NAME $CMAKE_ARGS || exit
cmake --build $BUILD_DIR --parallel $JOBS --target $TARGET $BUILD_ARGS
