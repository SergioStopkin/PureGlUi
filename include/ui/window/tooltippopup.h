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

#include "common/noncopyable.h"
#include "common/time.h"
#include "ui/config.h"
#include "ui/idkind.h"
#include "ui/render/uielementstate.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/button.h"
#include "ui/res/type/menu.h"
#include "ui/type.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/pairing.h"
#include "ui/window/connector/tooltipconnector.h"
#include "ui/window/geometry.h"
#include "ui/window/hostsurfaces.h"
#include "ui/window/istooltipdelayed.h"
#include "ui/window/popupslot.h"
#include "ui/window/popupwindows.h"
#include "ui/window/renderqueue.h"
#include "ui/window/spentbuttonof.h"
#include "ui/window/tooltip.h"
#include "ui/window/tooltipboundof.h"
#include "ui/window/tooltipwantedof.h"
#include "ui/window/underlay.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

namespace Ui::Window {

// Flip to true to time every tooltip change - one line per close and per show,
// the kept window moved or a new one made - and every frame drawn while one is
// open, which should hold one vsync (16.7 ms at 60 Hz, 11.1 at 90), not two
constexpr bool TOOLTIP_DEBUG = false;

// The tooltip's behaviour: beside a toolbar or menu-bar button, or beside what a content
// surface's host marks, opened and closed after every event and frame (sync); none while
// a menu or dialog is open. It owns no window: the WindowManager's tooltip slot is handed
// in, as every other popup part's
class TooltipPopup final : private Common::NonCopyable {
public:
    // All the window manager's, `main` empty until it makes the window
    TooltipPopup(const Ui::Res::ResManager &            resManager,
                 const std::unique_ptr<MainConnector> & main,
                 const Geometry &                       geometry,
                 const HostSurfaces &                   hostSurfaces,
                 PopupWindows &                         popups,
                 Underlay &                             underlay,
                 popup_slot_t<TooltipConnector> &       slot,
                 Ui::Window::RenderQueue &              renderQueue)
        : m_resManager(resManager)
        , m_main(main)
        , m_geometry(geometry)
        , m_hostSurfaces(hostSurfaces)
        , m_popups(popups)
        , m_underlay(underlay)
        , m_slot(slot)
        , m_renderQueue(renderQueue)
    {
    }

    // Open, keep or close the tooltip (spentButtonOf, tooltipWantedOf); none while a menu
    // or dialog is open, `isChromeOpen`. The window stays while what it shows does
    void sync(bool isChromeOpen)
    {
        const bool                    hasRenderer = m_main && m_main->hasRenderer();
        const Ui::Render::UiElement * button      = (hasRenderer && !isChromeOpen) ? m_main->renderer().hoveredButton()
                                                                                   : nullptr;
        const id_t                    hoveredButton = (button != nullptr) ? button->id : Ui::INVALID_ID;
        // A press silences its tooltip; only a pointer move off it ends that (followPointer)
        if (button != nullptr && button->state == Ui::Render::UiElementState::Active) {
            m_spentButton = hoveredButton;
            m_spentBound  = button->bound;
        }
        const tooltip_t wanted = tooltipWantedOf(
            (button != nullptr) ? tooltip_t { .text = textOf(hoveredButton), .anchor = button->bound } : tooltip_t {},
            hoveredButton,
            m_spentButton,
            isChromeOpen ? tooltip_t {} : m_hostSurfaces.tooltip());
        // Held for showDue
        if (isTooltipDelayed(wanted, hoveredButton, m_tooltip)) {
            if (wanted != m_pending) {
                m_pending = wanted;
                m_due     = Common::Time::now() + std::chrono::milliseconds(m_resManager.input().tooltipDelayMs);
            }
            return;
        }
        m_pending = {};
        if (wanted == m_tooltip) {
            return;
        }
        [[maybe_unused]] const auto closing = Common::Time::now();
        hide();
        if constexpr (TOOLTIP_DEBUG) {
            std::cout << "[WindowManager] tooltip closed in " << Common::Time::msSince(closing) << " ms" << std::endl;
        }
        if (!wanted.text.empty()) {
            show(wanted);
        }
    }

    // A toolbar button's tooltip held for its delay, shown once that has run out; dropped
    // while a menu or dialog is open, `isChromeOpen`
    void showDue(bool isChromeOpen)
    {
        if (isChromeOpen) {
            m_pending = {};
        }
        if (m_pending.text.empty() || Common::Time::now() < m_due) {
            return;
        }
        show(m_pending);
        m_pending = {};
    }

    // A tooltip is wanted - presented or not yet (markPresented)
    [[nodiscard]] bool isWanted() const { return !m_tooltip.text.empty(); }

    // The pointer moved to `cssX`, `cssY` on the main window: off the button a press
    // silenced, its tooltip may show again
    void followPointer(fpx_t cssX, fpx_t cssY)
    {
        m_spentButton = spentButtonOf(m_spentButton, m_spentBound, cssX, cssY);
    }

    // The pointer moved onto a content surface, so off every button
    void leaveButtons() { m_spentButton = Ui::INVALID_ID; }

    // Its frame presented: shown once its first is, corners and all - never flat.
    // Composited it is drawn into the main window and never shown
    void markPresented()
    {
        if (!m_isPresented && !g_config.isCompositing) {
            m_slot.connector->window().show();
        }
        m_isPresented = true;
    }

    // Closes its window and forgets what it showed too, so the next sync shows it again if wanted
    void destroy()
    {
        m_tooltip     = {};
        m_pending     = {};
        m_isPresented = false;
        if (!m_slot.connector) {
            return;
        }
        m_popups.close(m_slot);
        // Composited, it is only a texture on the main window, gone with its next frame
        m_renderQueue.request(MAIN_WINDOW_ID);
    }

private:
    const Ui::Res::ResManager &            m_resManager;
    const std::unique_ptr<MainConnector> & m_main;
    const Geometry &                       m_geometry;
    const HostSurfaces &                   m_hostSurfaces;
    PopupWindows &                         m_popups;
    Underlay &                             m_underlay;
    popup_slot_t<TooltipConnector> &       m_slot;
    Ui::Window::RenderQueue &              m_renderQueue;
    tooltip_t                              m_tooltip;    // what the window shows, anchored in main-window CSS px
    tooltip_t                              m_pending;    // a toolbar button's, waiting out its delay (showDue)
    Ui::Res::Type::bound_t                 m_spentBound; // its rect, main-window CSS px (spentButtonOf)
    Common::Time::point_t                  m_due;        // when m_pending shows
    id_t m_spentButton = Ui::INVALID_ID;                 // a press silenced its tooltip until the pointer leaves it
    bool m_isPresented = false;                          // its first frame presented (markPresented)

    // A toolbar or menu-bar button's tooltip text, empty for one without. A menu-bar
    // button is a top-level menu, read in the index where it is rather than copied out
    // of it - this runs on every move over one
    [[nodiscard]] std::string textOf(id_t buttonId) const
    {
        std::string key;
        if (const Ui::Res::Type::button_t * button = m_resManager.findButton(buttonId)) {
            key = button->tooltip;
        }
        m_resManager.visitMenuItem(buttonId, [&key](const Ui::Res::Type::menu_t & menu) { key = menu.tooltip; });
        return key.empty() ? std::string {} : m_resManager.localeManager().get(key);
    }

    // Beside its anchor (tooltipBoundOf), drawn by the next frame and shown once it
    // has one (markPresented). The window made for the first is kept, moved and told
    // the new text while its size holds - a window, its EGL context and its font made
    // per snap was most of what a tooltip cost
    void show(const tooltip_t & tooltip)
    {
        [[maybe_unused]] const auto started = Common::Time::now();
        // Main context current: a glyph measured for the first time is uploaded
        // into the main renderer's atlas
        m_main->window().makeCurrent();
        const Ui::Res::Type::bound_t css   = tooltipBoundOf(tooltip.anchor,
                                                            m_resManager.layout().tooltip,
                                                            m_main->renderer().tooltipWidthOf(tooltip.text),
                                                            toCss(m_geometry.width()),
                                                            toCss(m_geometry.height()));
        const Ui::Res::Type::bound_t bound = toPhysRound(css);

        // Kept only at the size it was made: its window never presents (composited), so it
        // keeps the buffers it was made with, and a wider tooltip read back from it is cut
        if (m_slot.connector
            && (m_slot.connector->window().bound().w != bound.w || m_slot.connector->window().bound().h != bound.h)) {
            m_popups.close(m_slot);
            m_isPresented = false;
        }
        const bool isKept = m_slot.connector != nullptr;
        if (isKept) {
            // Composited it stays parked offscreen and only its bound moves, as when made
            auto & window = m_slot.connector->window();
            window.moveResize(g_config.isCompositing
                                  ? Ui::Res::Type::bound_t { .x = -bound.w, .y = -bound.h, .w = bound.w, .h = bound.h }
                                  : bound);
            window.setPosition(bound.x, bound.y);
            m_slot.connector->pairing().resize(bound.w, bound.h);
            m_slot.connector->renderer().setText(tooltip.text);
        } else if (!make(bound, tooltip.text)) {
            m_tooltip = tooltip; // counted as shown, or every frame would retry it
            return;
        }
        m_main->window().makeCurrent();
        m_tooltip = tooltip;

        // Drawn by the next frame, not here - a second main swap in one loop waits
        // on vsync: its main frame recaptures the corners (composited, draws it in)
        // and asks for the tooltip's own, after which it is shown (markPresented)
        m_renderQueue.request(MAIN_WINDOW_ID);
        if constexpr (TOOLTIP_DEBUG) {
            std::cout << "[WindowManager] tooltip " << (isKept ? "moved" : "made") << " in "
                      << Common::Time::msSince(started) << " ms" << std::endl;
        }
    }

    // The tooltip's window, `bound` in main-window physical px; false when it cannot be made
    bool make(const Ui::Res::Type::bound_t & bound, const std::string & text)
    {
        // Corners recaptured only while it is wanted, since a hidden one is kept for the next
        if (!m_popups.make(
                bound,
                m_resManager.layout().tooltip.border.scaled(g_config.scale),
                m_resManager.theme().tooltip.bg,
                true,
                [this]() {
                    if (isWanted()) {
                        m_underlay.recapture(m_slot);
                    }
                },
                m_slot)) {
            std::cerr << "[WindowManager] Failed to create tooltip window" << std::endl;
            return false;
        }
        m_popups.emplaceRenderer(m_slot, text);
        return true;
    }

    // Shows nothing, keeping the window for the next tooltip where it can move
    // (PopupWindow::isMovable). Composited, it was a texture on the main window, gone
    // with its next frame
    void hide()
    {
        m_tooltip     = {};
        m_isPresented = false;
        if (!m_slot.connector) {
            return;
        }
        if (!m_slot.connector->window().isMovable()) {
            destroy();
            return;
        }
        if (!g_config.isCompositing) {
            m_slot.connector->window().hide();
        }
        m_slot.composite.clear();
        m_hostSurfaces.redrawUnder(m_slot.connector->window().bound());
        m_renderQueue.request(MAIN_WINDOW_ID);
    }
};

} // namespace Ui::Window
