#!/usr/bin/env bash
. .cicd-config

sed -i 's/-static-libasan/ /g' $BUILD_DIR_DEV/compile_commands.json

# Filter out platform files that cannot compile on the current OS
TIDY_FILES="$SOURCE_FILES"
if [[ "`uname`" == "Linux" ]]; then
    TIDY_FILES=`echo "$TIDY_FILES" | grep -v 'win32\|macos'`
elif [[ "`uname`" == "Darwin" ]]; then
    TIDY_FILES=`echo "$TIDY_FILES" | grep -v 'win32\|x11\|wayland'`
fi

# File `$2`, number `$1` in the list: how long it took on fd 3, the terminal - a file a
# check sticks on stands out by its minutes - and, where clang-tidy exits non-zero, its
# code and all it said but the count of warnings it suppressed. Returns that code
TidyOne()
{
    local start=$SECONDS
    local said
    said=`clang-tidy$CLANG_VERSION --quiet -p $BUILD_DIR_DEV --header-filter="./include/${PROJECT_NAME}" "$2" 2>&1`
    local status=$?
    echo "[$1/$TIDY_TOTAL] $((SECONDS - start))s $2" >&3
    if [[ $status -ne 0 ]]; then
        echo "=== $2: exit $status"
        grep -Ev '^[0-9]+ warnings? generated\.$' <<< "$said"
    fi
    return $status
}
export -f TidyOne
TIDY_TOTAL=$((`wc -l <<< "$TIDY_FILES"`))
export CLANG_VERSION BUILD_DIR_DEV PROJECT_NAME TIDY_TOTAL

# The failing files' output once all are done, and xargs's own status: 123 when any
# clang-tidy exited non-zero, 125-127 when xargs could not run one
OUTPUT=`nl -w1 -s' ' <<< "$TIDY_FILES" | xargs -n 2 -P $JOBS bash -c 'TidyOne "$@"' _ 3>&2 2>&1`
RESULT=$?
if [[ -n "$OUTPUT" ]]; then
    echo "$OUTPUT"
fi

# echo $TEST_FILES | xargs clang-tidy --quiet -p $BUILD_DIR_DEV \
#     --checks '
#         -cert-err58-cpp,
#         -cppcoreguidelines-avoid-non-const-global-variables,
#         -cppcoreguidelines-owning-memory,
#         -fuchsia-default-arguments-calls,
#         -fuchsia-statically-constructed-objects'
# let "RESULT+=$?"

exit $RESULT
