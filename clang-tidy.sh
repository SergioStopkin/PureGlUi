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

OUTPUT=$(xargs -n 1 -P $JOBS clang-tidy$CLANG_VERSION --quiet -p $BUILD_DIR_DEV \
    --header-filter="./include/${PROJECT_NAME}" <<< "$TIDY_FILES" 2>&1)
# xargs exit status (captured before the grep below clobbers $?): 123 = a child
# exited 1-125 (a clang-tidy finding), 125 = a child was killed by a signal
# (clang-tidy crash). Either must fail the run - a text grep alone cannot see a
# crash, whose output is a stack dump with no ': error:' line.
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
