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
    # (dependency.sh / CI setup).
    if [[ -f "$MESA_DIR/x64/opengl32.dll" ]]; then
        exit 0
    fi
    SEVENZ="/c/Program Files/7-Zip/7z.exe"
    if [[ ! -x "$SEVENZ" ]]; then
        winget install -e --id 7zip.7zip || true
    fi
    MESA_URL=$(curl -fsS --retry 3 https://api.github.com/repos/pal1000/mesa-dist-win/releases/latest 2>/dev/null | grep -o 'https://[^"]*release-msvc.7z' | head -1)
    if [[ -x "$SEVENZ" ]] && [[ -n "$MESA_URL" ]] && curl -fL --retry 3 -o "$TEMP/mesa.7z" "$MESA_URL" && mkdir -p "$MESA_DIR" && "$SEVENZ" x -y "$TEMP/mesa.7z" -o"$MESA_DIR"; then
        rm -f "$TEMP/mesa.7z"
    else
        rm -f "$TEMP/mesa.7z"
        echo "[mesa] WARNING: Mesa fetch skipped or failed - optional, only needed on machines without a hardware GL driver. Re-run to retry." >&2
    fi
    exit 0
fi

# --- Deploy: copy Mesa next to the given exe dirs when no hardware ICD exists ---
# The ICD registry is the reliable signal: every real GL driver (GPU vendors,
# VM guest additions with 3D) registers OpenGLDriverName under the display-class
# key, while fallback adapters (Basic Display, QEMU VGA, Hyper-V Video) do not.
# Unlike adapter-name matching, this can neither override a real driver nor
# miss a driverless VM.

if [[ ! -f "$MESA_DIR/x64/opengl32.dll" ]]; then
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
if [[ "${ICD_COUNT:-0}" -ne 0 ]]; then
    exit 0
fi

for exe_dir in "${PENDING[@]}"; do
    echo "[mesa] No hardware OpenGL driver (ICD) registered - deploying Mesa software OpenGL to $exe_dir"
    cp "$MESA_DIR"/x64/*.dll "$exe_dir/"
done
