#!/usr/bin/env bash

DEPS_DIR=".github/dependencies"

if [[ "$OS" == "Windows_NT" ]]; then
    ./dependency-windows.sh
elif [[ "$(uname)" == "Darwin" ]]; then
    # Drop the runner's pre-installed, unused taps so brew does not warn that
    # they are untrusted on every install.
    brew untap aws/tap azure/bicep 2>/dev/null || true
    brew install $(grep -v '^#' $DEPS_DIR/macos)
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf makecache
    sudo dnf install -y $(grep -v '^#' $DEPS_DIR/linux-dnf)
else
    sudo apt-get update
    sudo apt-get install -y $(grep -v '^#' $DEPS_DIR/linux)
fi
