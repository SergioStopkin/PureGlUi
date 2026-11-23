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

#include "ui/config.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/layout.h"
#include "ui/type.h"

#include <algorithm>

namespace Ui::Window {

// The main window's size and the UI margins round the viewport, physical px: what a
// content surface, a dock and a dialog are placed by
class Geometry final {
public:
    [[nodiscard]] fpx_t width() const { return m_width; }
    [[nodiscard]] fpx_t height() const { return m_height; }

    void setSize(fpx_t width, fpx_t height)
    {
        m_width  = width;
        m_height = height;
    }

    // The chrome's margins - toolbars, menu bar with the tab strip, status bar - from
    // `layout`; the docks add theirs on top (addDock)
    void setChromeMargins(const Ui::Res::Type::layout_t & layout)
    {
        m_left   = toPhys(layout.leftToolbar.width);
        m_top    = toPhys(layout.topMenu.height) + toPhys(layout.workspaceTab.height);
        m_right  = toPhys(layout.rightToolbar.width);
        m_bottom = toPhys(layout.statusBar.height);
    }

    // The side margins back to the toolbars alone, the docks to be added again
    void resetSideMargins(const Ui::Res::Type::layout_t & layout)
    {
        m_left  = toPhys(layout.leftToolbar.width);
        m_right = toPhys(layout.rightToolbar.width);
    }

    // A dock `cssWidth` wide on the side it is anchored to
    void addDock(Ui::Res::Dock::DockAnchor anchor, fpx_t cssWidth)
    {
        const fpx_t width = toPhys(cssWidth);
        if (anchor == Ui::Res::Dock::DockAnchor::Left) {
            m_left += width;
        } else {
            m_right += width;
        }
    }

    [[nodiscard]] fpx_t left() const { return m_left; }
    [[nodiscard]] fpx_t top() const { return m_top; }
    [[nodiscard]] fpx_t right() const { return m_right; }
    [[nodiscard]] fpx_t bottom() const { return m_bottom; }

    // The rect left for embedded content after the UI chrome (toolbars + docks
    // + menu/tab/status). Content surfaces are sized/positioned to this.
    [[nodiscard]] Ui::Res::Type::bound_t viewport() const
    {
        return { .x = m_left,
                 .y = m_top,
                 .w = std::max(1.0F, m_width - m_left - m_right),
                 .h = std::max(1.0F, m_height - m_top - m_bottom) };
    }

private:
    fpx_t m_width  = 0;
    fpx_t m_height = 0;
    fpx_t m_left   = 0;
    fpx_t m_top    = 0;
    fpx_t m_right  = 0;
    fpx_t m_bottom = 0;
};

} // namespace Ui::Window
