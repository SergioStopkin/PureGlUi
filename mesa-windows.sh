#!/usr/bin/env bash

# Mesa software OpenGL for Windows machines without a hardware GL driver.
#
# Windows without a hardware GL driver (VMs, headless CI) exposes only
# Microsoft's software OpenGL 1.1, which cannot run our GL 3.3 pipeline.
# mesa-dist-win packages the official Mesa llvmpipe driver as a per-app
# opengl32.dll.
#
# Usage: ./mesa-windows.sh fetch                     - download Mesa into 3rd/mesa (gitignored)
#        ./mesa-windows.sh <exe-dir> [<exe-dir>...]  - deploy next to the exes, only if no hardware ICD
#
# No-op on non-Windows.

MESA_DIR="3rd/mesa"

if [[ "$OS" != "Windows_NT" ]]; then
    exit 0
fi

if [[ $1 == "fetch" ]]; then
    # Failures only WARN: Mesa is an optional runtime fallback, so a
    # rate-limited GitHub API or a network hiccup must not fail the caller
    # (dependency.sh / CI setup). Each step reports its own failure so a
    # skipped fetch is diagnosable from the log.
    if [[ -f "$MESA_DIR/x64/opengl32.dll" ]]; then
        echo "[mesa] Already fetched: $MESA_DIR"
        exit 0
    fi
    SEVENZ="/c/Program Files/7-Zip/7z.exe"
    if [[ ! -x "$SEVENZ" ]]; then
        winget install -e --id 7zip.7zip || true
    fi
    if [[ ! -x "$SEVENZ" ]]; then
        echo "[mesa] WARNING: 7-Zip not found at $SEVENZ (winget install failed?) - cannot extract Mesa" >&2
        exit 0
    fi
    # Resolve the latest version WITHOUT the GitHub API (unauthenticated API
    # calls are rate-limited per IP and fail silently): the releases/latest
    # page redirects to .../tag/<version>, and the release asset name is
    # stable: mesa3d-<version>-release-msvc.7z.
    LATEST_URL=$(curl -fsSI -o /dev/null -w '%{url_effective}' -L https://github.com/pal1000/mesa-dist-win/releases/latest 2>/dev/null)
    MESA_VERSION=${LATEST_URL##*/}
    if [[ -z "$MESA_VERSION" ]] || [[ "$MESA_VERSION" == "latest" ]]; then
        echo "[mesa] WARNING: could not resolve the latest mesa-dist-win version (network?) - re-run to retry" >&2
        exit 0
    fi
    MESA_URL="https://github.com/pal1000/mesa-dist-win/releases/download/${MESA_VERSION}/mesa3d-${MESA_VERSION}-release-msvc.7z"
    ARCHIVE="${TEMP:-/tmp}/mesa.7z"
    echo "[mesa] Downloading $MESA_URL"
    if ! curl -fL --retry 3 -o "$ARCHIVE" "$MESA_URL"; then
        rm -f "$ARCHIVE"
        echo "[mesa] WARNING: download failed - re-run to retry" >&2
        exit 0
    fi
    mkdir -p "$MESA_DIR"
    if ! "$SEVENZ" x -y "$ARCHIVE" -o"$MESA_DIR" > /dev/null; then
        rm -f "$ARCHIVE"
        echo "[mesa] WARNING: extraction failed" >&2
        exit 0
    fi
    rm -f "$ARCHIVE"
    echo "[mesa] Fetched into $MESA_DIR"
    exit 0
fi

# --- Deploy: copy Mesa next to the given exe dirs when no hardware ICD exists ---
# The ICD registry is the reliable signal: every real GL driver (GPU vendors,
# VM guest additions with 3D) registers OpenGLDriverName under the display-class
# key, while fallback adapters (Basic Display, QEMU VGA, Hyper-V Video) do not.
# Unlike adapter-name matching, this can neither override a real driver nor
# miss a driverless VM.
# Every skip path logs its reason - a silent no-op here surfaces later as a
# baffling GL 1.1 segfault in the app.

if [[ ! -f "$MESA_DIR/x64/opengl32.dll" ]]; then
    echo "[mesa] $MESA_DIR not present (run ./dependency.sh or ./mesa-windows.sh fetch) - skipping deploy"
    exit 0
fi

# Collect dirs that exist and are not yet deployed - if none, skip the query.
PENDING=()
for exe_dir in "$@"; do
    if [[ -d "$exe_dir" ]] && [[ ! -f "$exe_dir/opengl32.dll" ]]; then
        PENDING+=("$exe_dir")
    fi
done
if [[ ${#PENDING[@]} -eq 0 ]]; then
    exit 0
fi

# Count registered OpenGL ICDs: the per-adapter display-class key (modern) plus
# the legacy OpenGLDrivers key. Zero means only Microsoft's GL 1.1 is available.
ICD_COUNT=$(powershell -NoProfile -Command "\$icd = Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}' -ErrorAction SilentlyContinue | Get-ItemProperty -Name OpenGLDriverName -ErrorAction SilentlyContinue; \$legacy = Get-Item 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers' -ErrorAction SilentlyContinue; @(\$icd).Count + \$(if (\$legacy) { \$legacy.ValueCount + \$legacy.SubKeyCount } else { 0 })")
ICD_COUNT=${ICD_COUNT//[^0-9]/}
if [[ -z "$ICD_COUNT" ]]; then
    echo "[mesa] ICD registry query failed (powershell unavailable?) - skipping deploy" >&2
    exit 0
fi
if [[ "$ICD_COUNT" -ne 0 ]]; then
    echo "[mesa] Hardware OpenGL driver registered (ICDs: $ICD_COUNT) - not deploying software GL"
    exit 0
fi

for exe_dir in "${PENDING[@]}"; do
    echo "[mesa] No hardware OpenGL driver (ICD) registered - deploying Mesa software OpenGL to $exe_dir"
    cp "$MESA_DIR"/x64/*.dll "$exe_dir/"
done
