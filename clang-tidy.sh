#!/usr/bin/env bash
. .cicd-config

sed -i 's/-static-libasan/ /g' $BUILD_DIR_DEV/compile_commands.json

# Filter out platform files that cannot compile on the current OS
TIDY_FILES="$SOURCE_FILES"
if [[ "$(uname)" == "Linux" ]]; then
    TIDY_FILES=$(echo "$TIDY_FILES" | grep -v 'win32\|macos')
elif [[ "$(uname)" == "Darwin" ]]; then
    TIDY_FILES=$(echo "$TIDY_FILES" | grep -v 'win32\|x11\|wayland')
fi

# File `$2`, number `$1` in the list: its findings on stdout, collected below, and how
# long it took on fd 3, the terminal - a file a check sticks on stands out by its minutes.
# A crash prints a stack dump and no ': error:' line, so it is given one; the exit status
# is clang-tidy's
TidyOne()
{
    local start=$SECONDS
    clang-tidy$CLANG_VERSION --quiet -p $BUILD_DIR_DEV --header-filter="./include/${PROJECT_NAME}" "$2" 2>&1
    local status=$?
    if [[ $status -gt 128 ]]; then
        echo "$2: error: clang-tidy crashed (exit $status)"
    fi
    echo "[$1/$TIDY_TOTAL] $((SECONDS - start))s $2" >&3
    return $status
}
export -f TidyOne
TIDY_TOTAL=$(($(wc -l <<< "$TIDY_FILES")))
export CLANG_VERSION BUILD_DIR_DEV PROJECT_NAME TIDY_TOTAL

OUTPUT=$(nl -w1 -s' ' <<< "$TIDY_FILES" | xargs -n 2 -P $JOBS bash -c 'TidyOne "$@"' _ 3>&2 2>&1)
# xargs exit status (captured before the grep below clobbers $?): 123 = a clang-tidy
# run exited non-zero, with a finding or a crash (TidyOne hands on its status, 128 +
# the signal for a crash). Either must fail the run
TIDY_EXIT=$?

# Filter out false positives from vendored 3rd-party headers (nlohmann)
OUTPUT=$(echo "$OUTPUT" | grep -v '3rd/include/')

# Check for actual errors (ignore notes and warnings-generated lines)
ERRORS=$(echo "$OUTPUT" | grep ': error:' || true)

if [[ -n "$ERRORS" || $TIDY_EXIT -ne 0 ]]; then
    echo "$OUTPUT"
    if [[ $TIDY_EXIT -ne 0 ]]; then
        echo "[clang-tidy.sh] non-zero clang-tidy exit ($TIDY_EXIT): findings or a crash - see output above"
    fi
    RESULT=1
else
    RESULT=0
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
