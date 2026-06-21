#!/usr/bin/env bash

DEPS_DIR=".github/dependencies"

if [[ "$OS" == "Windows_NT" ]]; then
    vcpkg install $(grep -v '^#' $DEPS_DIR/windows) --triplet x64-windows-release
elif [[ "$(uname)" == "Darwin" ]]; then
    brew install $(grep -v '^#' $DEPS_DIR/macos)
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf makecache
    sudo dnf install -y $(grep -v '^#' $DEPS_DIR/linux-dnf)
else
    sudo apt-get update
    sudo apt-get install -y $(grep -v '^#' $DEPS_DIR/linux)
fi
