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

# lcov 2.x makes clang/gcov function-line mismatches and unused remove patterns
# fatal, and rejects the categories 1.16 lacks - so pass the ignore flags only
# when lcov is 2.x or newer.
#
# Each category is listed TWICE on purpose (lcov's own hint spells this out): the
# first occurrence downgrades the error to a warning, the second suppresses the
# warning as well. Listing once left hundreds of "function found on line but no
# corresponding 'line' coverage data point" lines in the CI log - unavoidable for
# a header-only library, where one-line inline accessors and implicit destructors
# of function-local types get a function record with no line record to derive an
# end line from. Do not "simplify" the duplicates away.
LCOV_MAJOR=$(lcov --version 2>/dev/null | grep -oE 'version [0-9]+' | grep -oE '[0-9]+')
LCOV_IGNORE=""
GENHTML_IGNORE=""
if [[ -n "$LCOV_MAJOR" && "$LCOV_MAJOR" -ge 2 ]]; then
    LCOV_IGNORE="--ignore-errors inconsistent,inconsistent,unused,unused"
    GENHTML_IGNORE="--ignore-errors inconsistent,inconsistent"
fi

lcov -q $LCOV_IGNORE -c --external --gcov-tool "$PWD/llvm-gcov.sh" -d $COV_RESULT_DIR -o $COV_RESULT_DIR/coverage.info
lcov -q $LCOV_IGNORE -r $COV_RESULT_DIR/coverage.info "/usr/*" -o $COV_RESULT_DIR/coverage.info
lcov -q $LCOV_IGNORE -r $COV_RESULT_DIR/coverage.info "*/3rd/*" -o $COV_RESULT_DIR/coverage.info
lcov -q $LCOV_IGNORE -r $COV_RESULT_DIR/coverage.info "*/interface/*" -o $COV_RESULT_DIR/coverage.info
lcov -q $LCOV_IGNORE -r $COV_RESULT_DIR/coverage.info "/*/$COV_SOURCE_DIR/*" -o $COV_RESULT_DIR/coverage.info

RESULT=$(genhtml $GENHTML_IGNORE -t "Unit Tests Coverage" --num-spaces 4 $COV_RESULT_DIR/coverage.info -o $COV_RESULT_DIR)
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

if [[ -z "$COVERAGE" ]]; then
    echo "Coverage: no line figure parsed - empty tracefile (see the lcov/geninfo errors above)"
    exit 1
fi

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
