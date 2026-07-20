#!/usr/bin/env bash
. .cicd-config

rm -rf $BUILD_DIR_DEV
rm -rf $BUILD_DIR_REL
rm -rf $BUILD_DIR_COV
rm -rf $RESULT_PREFIX-*

if [[ $1 == "3rd" ]]; then
    rm -rf $BUILD_DIR_3RD
fi
