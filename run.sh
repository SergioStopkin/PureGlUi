#!/usr/bin/env bash
. .cicd-config

USAGE_TEXT="Usage: ./run.sh <dev|rel|test|ut|ct> [args...]"

# One definition per suite; `test` runs both. Windows needs Mesa deployed next to
# the binary (software GL) and gtest's DLLs resolved from the build dir.
RunUnitTests()
{
    echo "_________________________________________    Unit Tests   _________________________________________"
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR_REL/test/unit/Release
        $BUILD_DIR_REL/test/unit/Release/unit-tests.exe "$@"
    else
        $BUILD_DIR_REL/test/unit/unit-tests "$@"
    fi
}

RunComponentTests()
{
    echo "_________________________________________ Component Tests _________________________________________"
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR_REL/test/component/Release
        $BUILD_DIR_REL/test/component/Release/component-tests.exe "$@"
    else
        $BUILD_DIR_REL/test/component/component-tests "$@"
    fi
}
if [[ $# -eq 0 ]]; then
    PrintUsageAndExit
elif [[ $1 == "dev" ]]; then
    export LSAN_OPTIONS=suppressions=.asan.supp
    shift  # Remove first argument (dev)
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR_DEV/src/Release
        $BUILD_DIR_DEV/src/Release/$TARGET_SRC.exe "$@"
    else
        $BUILD_DIR_DEV/src/$TARGET_SRC "$@"
    fi
elif [[ $1 == "rel" ]]; then
    shift  # Remove first argument (rel)
    if [[ "$OS" == "Windows_NT" ]]; then
        ./mesa-windows.sh $BUILD_DIR_REL/src/Release
        $BUILD_DIR_REL/src/Release/$TARGET_SRC.exe "$@"
    else
        $BUILD_DIR_REL/src/$TARGET_SRC "$@"
    fi
elif [[ $1 == "test" ]]; then
    shift  # Remove first argument (test)
    RunUnitTests "$@"
    RESULT=$?
    RunComponentTests "$@"
    let "RESULT+=$?"
    exit $RESULT
elif [[ $1 == "ut" ]]; then
    shift  # Remove first argument (ut)
    RunUnitTests "$@"
elif [[ $1 == "ct" ]]; then
    shift  # Remove first argument (ct)
    RunComponentTests "$@"
else
    PrintUsageAndExit
fi
