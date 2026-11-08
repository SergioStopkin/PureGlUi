#!/usr/bin/env bash
. .cicd-config

rm -rf $BUILD_DIR_DEV $BUILD_DIR_DEV$WAYLAND_SUFFIX
rm -rf $BUILD_DIR_REL $BUILD_DIR_REL$WAYLAND_SUFFIX
rm -rf $BUILD_DIR_COV $BUILD_DIR_COV$WAYLAND_SUFFIX
rm -rf $RESULT_PREFIX-*

if [[ $1 == "3rd" ]]; then
    rm -rf $BUILD_DIR_3RD
fi
