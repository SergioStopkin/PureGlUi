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
#include "ui/res/resmanager.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/dialog.h"
#include "ui/type.h"
#include "ui/window/nativewindow.h"
#include "ui/window/popup/popupwindow.h"

#include <iostream>

namespace Ui::Window::Popup {

using Ui::Window::NativeWindow;

/**
 * @brief Modal dialog window
 *
 * Owns the dialog-specific window logic: sizing, centering, creation.
 * The DialogRenderer pairing lives in the coordinator's Connector.
 */
class DialogWindow final : public PopupWindow {
public:
    explicit DialogWindow(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : PopupWindow(subscribe, subscribeId)
    {
    }

    ~DialogWindow() override = default;

    /**
     * @brief Open the dialog window centered on the parent (window only -
     * the coordinator emplaces the renderer on the connector afterwards)
     * @return true on success
     */
    bool open(NativeWindow &                  parentWindow,
              const Ui::Res::ResManager &     resManager,
              const Ui::Res::Type::dialog_t & dialog,
              const Ui::Res::Type::bound_t &  area)
    {
        const auto & dlg = resManager.layout().dialog;
        const auto   pos = centeredPosition(area,
                                            dialog.width > 0 ? dialog.width : dlg.width,
                                            dialog.height > 0 ? dialog.height : dlg.height);

        setPosition(pos.x, pos.y);
        setBackground(resManager.theme().dialog.bg);

        const Ui::Res::Type::border_t dialogRadii = dlg.border.scaled(g_config.scale);
        if (dialogRadii.anyNonZero()) {
            setCornerRadii(dialogRadii);
        }

        if (!create(parentWindow, pos.w, pos.h)) {
            std::cerr << "[DialogWindow] Failed to create window" << std::endl;
            return false;
        }

        if (g_config.isCompositing) {
            move(-pos.w, -pos.h);
            setPosition(pos.x, pos.y);
        }

        std::cout << "[DialogWindow] Opened dialog type=" << static_cast<int>(dialog.type) << std::endl;
        return true;
    }

    void recenter(const Ui::Res::Type::bound_t & area)
    {
        const auto pos = centeredPosition(area, toCss(m_bound.w), toCss(m_bound.h));
        if (g_config.isCompositing) {
            setPosition(pos.x, pos.y);
        } else {
            move(pos.x, pos.y);
        }
    }

private:
    // Centred in the area the coordinator hands over - the space inside the chrome, less
    // the dialog margin - not the raw window: CSS px in, physical px out. Its edges on a
    // whole CSS px (toPhysEdge): whole pixels, which the composite texture on XWayland
    // needs, and at macOS's 2x whole points - AppKit widens a window standing on half a
    // point out to whole ones, a point taller than GL draws, left black along its top.
    // Its size is the pixels between those edges: at 1.25x/1.5x/1.75x a fraction the
    // platform would cut off, ending the window short of where GL draws its far edge
    static Ui::Res::Type::bound_t centeredPosition(const Ui::Res::Type::bound_t & area, fpx_t cssW, fpx_t cssH)
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
