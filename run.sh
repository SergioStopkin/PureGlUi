#!/usr/bin/env bash
. .cicd-config

USAGE_TEXT="Usage: ./run.sh <dev|rel|test|ut|ct> [wl] [args...]"

if [[ $# -eq 0 ]]; then
    PrintUsageAndExit
fi
MODE=$1
shift
# The same mode's native Wayland build, beside it
SUFFIX=""
if [[ $1 == "wl" ]]; then
    if [[ "`uname`" != "Linux" ]]; then
        echo "wl: native Wayland is Linux only"
        exit 1
    fi
    SUFFIX=$WAYLAND_SUFFIX
    shift
fi
BUILD_DIR=$BUILD_DIR_REL$SUFFIX

# One definition per suite; `test` runs both. Windows needs Mesa deployed next to
# the binary (software GL) and gtest's DLLs resolved from the build dir.
RunUnitTests()
{
    echo "_________________________________________    Unit Tests   _________________________________________"
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR/test/unit/Release
        $BUILD_DIR/test/unit/Release/unit-tests.exe "$@"
    else
        $BUILD_DIR/test/unit/unit-tests "$@"
    fi
}

RunComponentTests()
{
    echo "_________________________________________ Component Tests _________________________________________"
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR/test/component/Release
        $BUILD_DIR/test/component/Release/component-tests.exe "$@"
    else
        $BUILD_DIR/test/component/component-tests "$@"
    fi
}
if [[ $MODE == "dev" ]]; then
    export LSAN_OPTIONS=suppressions=.asan.supp
    # Mesa's software driver keeps its runtime-generated x86 code in a heap only a static of
    # its own points at, and eglTerminate unloads the driver, so the leak check finds the heap
    # orphaned. Its generic C path makes none; the generated code is uninstrumented anyway
    export GALLIUM_NOSSE=1
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR_DEV$SUFFIX/src/Release
        $BUILD_DIR_DEV$SUFFIX/src/Release/$TARGET_SRC.exe "$@"
    else
        $BUILD_DIR_DEV$SUFFIX/src/$TARGET_SRC "$@"
    fi
elif [[ $MODE == "rel" ]]; then
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR/src/Release
        $BUILD_DIR/src/Release/$TARGET_SRC.exe "$@"
    else
        $BUILD_DIR/src/$TARGET_SRC "$@"
    fi
elif [[ $MODE == "test" ]]; then
    RunUnitTests "$@"
    RESULT=$?
    RunComponentTests "$@"
    let "RESULT+=$?"
    exit $RESULT
elif [[ $MODE == "ut" ]]; then
    RunUnitTests "$@"
elif [[ $MODE == "ct" ]]; then
    RunComponentTests "$@"
else
    PrintUsageAndExit
fi
