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

#include <cstdint>

namespace Ui::Window {

// Multi-click burst counter for platforms whose mouse events don't carry a
// native click count (X11, Wayland, raw Win32 WM_LBUTTONDOWN). Each call to
// next() returns the position of the new press in its burst: 1 for a fresh
// click, 2 for a double, 3 for a triple, and so on. Two presses belong to
// the same burst when they share a button, fall within m_intervalMs, and
// land within m_distancePx of each other. Thresholds come from
// res/input.json via configure(); defaults track typical desktop values
// so the helper is usable before the JSON load completes.
//
// macOS supplies NSEvent.clickCount directly and does not need this helper.
struct alignas(64) ClickCounter final {
    void configure(uint32_t intervalMs, int distancePx)
    {
        m_intervalMs = intervalMs;
        m_distancePx = distancePx;
    }

    int next(uint32_t timeMs, int x, int y, uint32_t button)
    {
        const uint32_t elapsed = timeMs - m_lastTime;
        const int      dx      = x - m_lastX;
        const int      dy      = y - m_lastY;
        const bool     isNear  = (dx * dx + dy * dy) <= (m_distancePx * m_distancePx);
        // Explicit m_hasPrior gate: using m_lastTime==0 as the "no prior
        // click" sentinel would misclassify a genuine timestamp of 0
        // (Wayland uses ms since an arbitrary epoch, X11 timestamps wrap)
        // as a fresh burst.
        const int count = (m_hasPrior && button == m_lastButton && elapsed <= m_intervalMs && isNear) ? m_lastCount + 1
                                                                                                      : 1;
        m_lastTime      = timeMs;
        m_lastX         = x;
        m_lastY         = y;
        m_lastButton    = button;
        m_lastCount     = count;
        m_hasPrior      = true;
        return count;
    }

private:
    uint32_t m_intervalMs = 400;
    int      m_distancePx = 5;
    uint32_t m_lastTime   = 0;
    int      m_lastX      = 0;
    int      m_lastY      = 0;
    uint32_t m_lastButton = 0;
    int      m_lastCount  = 0;
    bool     m_hasPrior   = false; // false until the first next() call
};

} // namespace Ui::Window
