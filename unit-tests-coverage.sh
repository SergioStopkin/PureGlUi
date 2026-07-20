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
if (( `echo "$COVERAGE < $COV_MIN_LIMIT" | sed 's/%//g' | bc -l` )); then
    echo "Unit tests coverage $COVERAGE less then $COV_MIN_LIMIT"
    exit 1
else
    echo "Unit tests coverage $COVERAGE"
    exit 0
fi
