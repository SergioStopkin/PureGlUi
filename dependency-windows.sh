#!/usr/bin/env bash

# Windows host setup - everything after "install Git" is automated here.
# Called by dependency.sh on Windows_NT (run from Git Bash). Safe to re-run:
# every step is guarded and skips work already done. winget installs pop a
# UAC prompt when elevation is needed - that is the only user interaction.

set -e

DEPS_DIR=".github/dependencies"

# --- Visual Studio 2022 Build Tools (MSVC + Windows SDK) ---
# vswhere ships with any VS installer and is the canonical MSVC detector.
# Two distinct gaps need two different actions: no VS at all -> winget install;
# VS present but the C++ workload missing -> the VS installer's modify verb.
# (winget install on an already-installed id takes the upgrade path, ignores
# --override, and exits nonzero without ever adding the workload.)
VS_INSTALLER_DIR="/c/Program Files (x86)/Microsoft Visual Studio/Installer"
VSWHERE="$VS_INSTALLER_DIR/vswhere.exe"
HAS_MSVC=""
VS_PATH=""
if [[ -x "$VSWHERE" ]]; then
    HAS_MSVC=$("$VSWHERE" -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath || true)
    VS_PATH=$("$VSWHERE" -products '*' -latest -property installationPath || true)
fi
if [[ -z "$HAS_MSVC" ]]; then
    if [[ -n "$VS_PATH" ]]; then
        "$VS_INSTALLER_DIR/setup.exe" modify --installPath "$VS_PATH" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart --wait
    else
        winget install -e --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    fi
fi

# --- cmake on the global PATH ---
# VS bundles a cmake but only on PATH inside its dev prompt; Git Bash needs one
# on the system PATH (mirrors macOS installing cmake via brew). A winget-fresh
# install becomes visible in a NEWLY opened shell.
command -v cmake >/dev/null 2>&1 || winget install -e --id Kitware.CMake

# --- vcpkg (the Windows equivalent of apt/brew for C++ libraries) ---
# Bootstrapped into C:\vcpkg - the path build.sh hardcodes as the CMake
# toolchain. Pre-installed on CI runners, so the clone is skipped there.
VCPKG_DIR="/c/vcpkg"
if [[ ! -x "$VCPKG_DIR/vcpkg.exe" ]]; then
    git clone https://github.com/microsoft/vcpkg "$VCPKG_DIR"
    "$VCPKG_DIR/bootstrap-vcpkg.bat"
fi
"$VCPKG_DIR/vcpkg.exe" install $(grep -v '^#' $DEPS_DIR/windows) --triplet x64-windows-release

# --- Mesa (software OpenGL driver) into 3rd/mesa (gitignored) ---
# All Mesa logic (fetch + conditional deploy) lives in mesa-windows.sh; the
# fetch is guarded (skips when already present) and warn-only on failure.
./mesa-windows.sh fetch
