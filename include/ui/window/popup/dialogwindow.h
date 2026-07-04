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
#include "ui/render/popup/dialogrenderer.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/dialog.h"
#include "ui/window/popup/popupwindow.h"

#include <functional>
#include <iostream>
#include <memory>

namespace Ui::Window::Popup {

using Ui::Window::NativeWindow;

/**
 * @brief Modal dialog window
 *
 * Owns all dialog-specific logic: sizing, centering, renderer init,
 * close callback. WindowManager just creates/destroys this.
 */
class DialogWindow final : public PopupWindow {
public:
    using CloseFn = Ui::Render::Popup::DialogRenderer::DialogCloseFn; // one definition of the close signature

    explicit DialogWindow(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : PopupWindow(subscribe, subscribeId)
    {
    }

    // DialogWindow owns its renderer as DialogRenderer; the base drives it through
    // these overrides, so nothing downcasts.
    [[nodiscard]] bool                 hasRenderer() const override { return m_dialogRenderer != nullptr; }
    [[nodiscard]] Ui::IRenderer &      activeRenderer() override { return *m_dialogRenderer; }
    [[nodiscard]] Ui::IPopupRenderer & popupRenderer() override { return *m_dialogRenderer; }

    [[nodiscard]] Ui::Render::Popup::DialogRenderer & dialogRenderer() { return *m_dialogRenderer; }

    ~DialogWindow() override = default;

    /**
     * @brief Open the dialog centered on the parent window
     * @return true on success
     */
    bool open(NativeWindow &                  parentWindow,
              const Ui::Res::ResManager &     resManager,
              const Ui::Res::Type::dialog_t & dialog,
              CloseFn                         onClose)
    {
        const auto & dlg     = resManager.layout().dialog;
        const fpx_t  dialogW = toPhys(dialog.width > 0 ? dialog.width : dlg.width);
        const fpx_t  dialogH = toPhys(dialog.height > 0 ? dialog.height : dlg.height);

        const auto & layout   = resManager.layout();
        const fpx_t  uiTop    = toPhys(layout.topMenu.height) + toPhys(layout.workspaceTab.height);
        const fpx_t  uiBottom = toPhys(layout.statusBar.height);
        const auto   pos      = centeredPosition(parentWindow.bound(), dialogW, dialogH, uiTop, uiBottom);

        setPosition(pos.x, pos.y);
        setBackground(resManager.theme().dialog.bg);

        const Ui::Res::Type::border_t dialogRadii = dlg.border.scaled(g_config.scale);
        if (dialogRadii.anyNonZero()) {
            setCornerRadii(dialogRadii);
        }

        if (!create(parentWindow, dialogW, dialogH)) {
            std::cerr << "[DialogWindow] Failed to create window" << std::endl;
            return false;
        }

        if (g_config.isCompositing) {
            move(-dialogW, -dialogH);
            setPosition(pos.x, pos.y);
        }

        makeCurrent();
        m_dialogRenderer = std::make_unique<Ui::Render::Popup::DialogRenderer>([this] { makeCurrent(); },
                                                                               resManager,
                                                                               dialog);
        m_dialogRenderer->resize(m_bound.w, m_bound.h);
        m_dialogRenderer->setAlpha(hasAlpha());
        m_dialogRenderer->setCloseCallback(std::move(onClose));

        std::cout << "[DialogWindow] Opened dialog type=" << static_cast<int>(dialog.type) << std::endl;
        return true;
    }

    void recenter(const Ui::Res::Type::bound_t & parentBound, fpx_t uiTop, fpx_t uiBottom)
    {
        const auto pos = centeredPosition(parentBound, m_bound.w, m_bound.h, uiTop, uiBottom);
        if (g_config.isCompositing) {
            setPosition(pos.x, pos.y);
            if (hasRenderer()) {
                activeRenderer().move();
            }
        } else {
            move(pos.x, pos.y);
        }
    }

private:
    // Compute centered position snapped to integer pixels
    // to avoid sub-pixel composite texture misalignment on XWayland
    static Ui::Res::Type::bound_t
    centeredPosition(const Ui::Res::Type::bound_t & parent, fpx_t dialogW, fpx_t dialogH, fpx_t uiTop, fpx_t uiBottom)
    {
        const fpx_t contentH = parent.h - uiTop - uiBottom;
        return { std::round((parent.w - dialogW) / 2.0F),
                 std::round(uiTop + (contentH - dialogH) / 2.0F),
                 dialogW,
                 dialogH };
    }

    std::unique_ptr<Ui::Render::Popup::DialogRenderer> m_dialogRenderer;
};

} // namespace Ui::Window::Popup
