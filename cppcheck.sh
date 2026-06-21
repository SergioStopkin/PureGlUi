#!/usr/bin/env bash
. .cicd-config

cppcheck \
    --quiet \
    --enable=all \
    --inline-suppr \
    --suppressions-list=.cppcheck.supp \
    --error-exitcode=1 \
    --platform=unix64 \
    $CPPCHECK_SOURCE_DIR
