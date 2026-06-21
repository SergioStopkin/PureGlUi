// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureGlUi. PureGlUi is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureGlUi is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureGlUi. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "ui/type.h"

#include <cmath>

namespace Ui {

// Display configuration the host hands the fw, so fw rendering reads scale/dpi
// from here rather than a process global. Owns the CSS <-> physical pixel math.
struct alignas(16) config_t final {
    int   dpi           = 96;    // physical dots per inch
    fpx_t scale         = 1.0F;  // physical px per CSS px (dpi / 96)
    bool  isCompositing = false; // UI composited into one surface (XWayland workaround)

    // CSS px -> physical px. toPhys: raw scale; toPhysFloor: floor to int (font/
    // texture sizes); toPhysRound: round CSS to int before scaling so the result
    // lands on the physical pixel grid (popup pos/size).
    [[nodiscard]] fpx_t toPhys(int css) const { return css * scale; }
    [[nodiscard]] fpx_t toPhys(fpx_t css) const { return css * scale; }
    [[nodiscard]] int   toPhysFloor(int css) const { return static_cast<int>(css * scale); }
    [[nodiscard]] int   toPhysFloor(fpx_t css) const { return static_cast<int>(css * scale); }
    [[nodiscard]] fpx_t toPhysRound(int css) const { return css * scale; }
    [[nodiscard]] fpx_t toPhysRound(fpx_t css) const { return static_cast<fpx_t>(std::lround(css)) * scale; }

    // physical px -> CSS px.
    [[nodiscard]] fpx_t toCss(int phys) const { return phys / scale; }
    [[nodiscard]] fpx_t toCss(fpx_t phys) const { return phys / scale; }
    [[nodiscard]] int   toCssFloor(int phys) const { return static_cast<int>(phys / scale); }
    [[nodiscard]] int   toCssFloor(fpx_t phys) const { return static_cast<int>(phys / scale); }
};

// Canonical display config the host updates, so fw rendering reads scale/dpi
// from here instead of a process global. The free-function shims keep call
// sites spelling toPhys()/toCss()/roundToInt() plainly.
inline config_t g_config;

inline int roundToInt(fpx_t v) { return static_cast<int>(std::lround(v)); }
inline int roundToInt(double v) { return static_cast<int>(std::lround(v)); }

inline fpx_t toPhys(int css) { return g_config.toPhys(css); }
inline fpx_t toPhys(fpx_t css) { return g_config.toPhys(css); }
inline int   toPhysFloor(int css) { return g_config.toPhysFloor(css); }
inline int   toPhysFloor(fpx_t css) { return g_config.toPhysFloor(css); }
inline fpx_t toPhysRound(int css) { return g_config.toPhysRound(css); }
inline fpx_t toPhysRound(fpx_t css) { return g_config.toPhysRound(css); }
inline fpx_t toCss(int phys) { return g_config.toCss(phys); }
inline fpx_t toCss(fpx_t phys) { return g_config.toCss(phys); }
inline int   toCssFloor(int phys) { return g_config.toCssFloor(phys); }
inline int   toCssFloor(fpx_t phys) { return g_config.toCssFloor(phys); }

} // namespace Ui
