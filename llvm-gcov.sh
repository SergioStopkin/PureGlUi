#!/usr/bin/env bash

# lcov's default gcov is GNU gcov, which cannot read clang's coverage data.
# Route lcov (--gcov-tool) through llvm-cov's gcov emulation instead.
# LLVM_COV is exported by unit-tests-coverage.sh (versioned, e.g. llvm-cov-16).
exec ${LLVM_COV:-llvm-cov} gcov "$@"
