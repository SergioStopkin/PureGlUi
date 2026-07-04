#!/usr/bin/env bash
. .cicd-config

USAGE_TEXT="Usage: ./run.sh <dev|rel|test> [args...]"
if [[ $# -eq 0 ]]; then
    PrintUsageAndExit
elif [[ $1 == "dev" ]]; then
    export LSAN_OPTIONS=suppressions=.asan.supp
    shift  # Remove first argument (dev)
    if [[ "$OS" == "Windows_NT" ]]; then
        $BUILD_DIR_DEV/src/Release/$TARGET_SRC.exe "$@"
    else
        $BUILD_DIR_DEV/src/$TARGET_SRC "$@"
    fi
elif [[ $1 == "rel" ]]; then
    shift  # Remove first argument (rel)
    if [[ "$OS" == "Windows_NT" ]]; then
        $BUILD_DIR_REL/src/Release/$TARGET_SRC.exe "$@"
    else
        $BUILD_DIR_REL/src/$TARGET_SRC "$@"
    fi
elif [[ $1 == "test" ]]; then
    shift  # Remove first argument (test)
    echo "_________________________________________    Unit Tests   _________________________________________"
    if [[ "$OS" == "Windows_NT" ]]; then
        $BUILD_DIR_REL/test/unit/Release/unit-tests.exe "$@"
    else
        $BUILD_DIR_REL/test/unit/unit-tests "$@"
    fi
    RESULT=$?
    echo "_________________________________________ Component Tests _________________________________________"
    if [[ "$OS" == "Windows_NT" ]]; then
        $BUILD_DIR_REL/test/component/Release/component-tests.exe "$@"
    else
        $BUILD_DIR_REL/test/component/component-tests "$@"
    fi
    let "RESULT+=$?"
    exit $RESULT
else
    PrintUsageAndExit
fi
