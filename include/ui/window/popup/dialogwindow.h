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
#include "ui/pubsub/subscribe.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"
#include "ui/window/popup/popupwindow.h"

namespace Ui::Window::Popup {

/**
 * @brief Modal dialog window
 *
 * A popup-type window like every other, made as they are (PopupWindows::make), that a
 * window manager may give the focus: a click on it, coming back from another app, has
 * to give it the keys. Centred in the area the coordinator hands over.
 */
class DialogWindow final : public PopupWindow {
public:
    explicit DialogWindow(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : PopupWindow(subscribe, subscribeId)
    {
        setFocusable(true);
    }

    ~DialogWindow() override = default;

    void recenter(const Ui::Res::Type::bound_t & area)
    {
        const auto pos = centeredBoundOf(area, toCss(m_bound.w), toCss(m_bound.h));
        if (g_config.isCompositing) {
            setPosition(pos.x, pos.y);
        } else {
            move(pos.x, pos.y);
        }
    }

    // Centred in the area the coordinator hands over - the space inside the chrome, less
    // the dialog margin - not the raw window: CSS px in, physical px out. Its edges on a
    // whole CSS px (toPhysEdge): whole pixels, which the composite texture on XWayland
    // needs, and at macOS's 2x whole points - AppKit widens a window standing on half a
    // point out to whole ones, a point taller than GL draws, left black along its top.
    // Its size is the pixels between those edges: at 1.25x/1.5x/1.75x a fraction the
    // platform would cut off, ending the window short of where GL draws its far edge
    [[nodiscard]] static Ui::Res::Type::bound_t centeredBoundOf(const Ui::Res::Type::bound_t & area,
                                                                fpx_t                          cssW,
                                                                fpx_t                          cssH)
    {
        const fpx_t left = area.x + ((area.w - cssW) / 2.0F);
        const fpx_t top  = area.y + ((area.h - cssH) / 2.0F);
        return { .x = toPhysEdge(left),
                 .y = toPhysEdge(top),
                 .w = toPhysEdge(left + cssW) - toPhysEdge(left),
                 .h = toPhysEdge(top + cssH) - toPhysEdge(top) };
    }
};

} // namespace Ui::Window::Popup
