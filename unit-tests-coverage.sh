#!/usr/bin/env bash
. .cicd-config

export LLVM_COV="llvm-cov$CLANG_VERSION"

# lcov 1.16's geninfo emits harmless Perl "Subroutine ... redefined" warnings; drop just those.
exec 2> >(grep -v 'Subroutine .* redefined' >&2)

TEST_BIN=$BUILD_DIR_COV/$COV_SOURCE_DIR/unit-tests
if [[ ! -x $TEST_BIN ]]; then
    echo "Coverage: $TEST_BIN not found - run ./build.sh cov first"
    exit 1
fi
$TEST_BIN

rm -rf $COV_RESULT_DIR
mkdir $COV_RESULT_DIR

find $BUILD_DIR_COV/$COV_SOURCE_DIR -name "*.gcno" -exec cp "{}" $COV_RESULT_DIR/ \;
find $BUILD_DIR_COV/$COV_SOURCE_DIR -name "*.gcda" -exec cp "{}" $COV_RESULT_DIR/ \;

lcov -q -c --external --gcov-tool "$PWD/llvm-gcov.sh" -d $COV_RESULT_DIR -o $COV_RESULT_DIR/coverage.info
lcov -q -r $COV_RESULT_DIR/coverage.info "/usr/*" -o $COV_RESULT_DIR/coverage.info
lcov -q -r $COV_RESULT_DIR/coverage.info "*/3rd/*" -o $COV_RESULT_DIR/coverage.info
lcov -q -r $COV_RESULT_DIR/coverage.info "*/interface/*" -o $COV_RESULT_DIR/coverage.info
lcov -q -r $COV_RESULT_DIR/coverage.info "/*/$COV_SOURCE_DIR/*" -o $COV_RESULT_DIR/coverage.info

RESULT=$(genhtml -t "Unit Tests Coverage" --num-spaces 4 $COV_RESULT_DIR/coverage.info -o $COV_RESULT_DIR)
IS_FOUND=0

for item in $RESULT
do
    if [[ $IS_FOUND == 1 ]]; then
        COVERAGE=$item
        break
    fi
    if [[ $item == "lines......:" ]]; then
        IS_FOUND=1
    fi
done

echo

STATUS=0

if (( `echo "$COVERAGE < $COV_MIN_LIMIT" | sed 's/%//g' | bc -l` )); then
    echo "Unit tests coverage $COVERAGE less then $COV_MIN_LIMIT"
    STATUS=1
else
    echo "Unit tests coverage $COVERAGE"
fi

# Per-file function coverage must meet COV_FILE_FUNC_LIMIT (interface contracts,
# whose only "functions" are uncoverable destructor ABI variants, are excluded above).
UNCOVERED=$(awk -v limit=${COV_FILE_FUNC_LIMIT%\%} '
/^SF:/  { file = substr($0, 4); fnf = 0; fnh = 0 }
/^FNF:/ { fnf = substr($0, 5) + 0 }
/^FNH:/ { fnh = substr($0, 5) + 0 }
/^end_of_record/ {
    funcpct = (fnf > 0) ? (100.0 * fnh / fnf) : 100.0
    if (fnf > 0 && funcpct < limit) { printf "  %s: %d/%d functions (%.1f%%)\n", file, fnh, fnf, funcpct }
}
' $COV_RESULT_DIR/coverage.info)

if [[ -n "$UNCOVERED" ]]; then
    echo
    echo "Files below $COV_FILE_FUNC_LIMIT function coverage:"
    echo "$UNCOVERED"
    STATUS=1
fi

exit $STATUS
