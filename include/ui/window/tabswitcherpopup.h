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
#include "ui/config.h"
#include "ui/const.h"
#include "ui/idkind.h"
#include "ui/render/popup/tabswitcherrenderer.h"
#include "ui/render/rowsshownof.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/tab.h"
#include "ui/type.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/tabswitcherconnector.h"
#include "ui/window/geometry.h"
#include "ui/window/mainframe.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/popupslot.h"
#include "ui/window/popupwindows.h"
#include "ui/window/renderqueue.h"
#include "ui/window/tabswitcherboundof.h"
#include "ui/window/underlay.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace Ui::Window {

// The Ctrl+Tab list's window: made over the main window's middle, its rows fitted, brought
// up, highlighted and closed. The window is the WindowManager's, a slot it hands in, with
// where the pointer is (main-window physical px, NO_POINTER off it) and what the pointer
// does once the list is gone (`rehover`); which tabs it lists is the shell's (Ui::TabSwitcher)
class TabSwitcherPopup final : private Common::NonCopyable {
public:
    TabSwitcherPopup(const Ui::Res::ResManager &            resManager,
                     const std::unique_ptr<MainConnector> & main,
                     const Geometry &                       geometry,
                     const int &                            pointerX,
                     const int &                            pointerY,
                     PopupWindows &                         popups,
                     Underlay &                             underlay,
                     MainFrame &                            mainFrame,
                     popup_slot_t<TabSwitcherConnector> &   slot,
                     Ui::Window::RenderQueue &              renderQueue,
                     Ui::task_fn_t                          rehover)
        : m_resManager(resManager)
        , m_main(main)
        , m_geometry(geometry)
        , m_pointerX(pointerX)
        , m_pointerY(pointerY)
        , m_popups(popups)
        , m_underlay(underlay)
        , m_mainFrame(mainFrame)
        , m_slot(slot)
        , m_renderQueue(renderQueue)
        , m_rehover(std::move(rehover))
    {
    }

    // The list over the main window's middle: `rows` most recent first, fitted to the
    // window, as many shown as fit, `selectedRow` highlighted. Brought up as a menu's popup
    // is: corners captured off a fresh main frame and drawn in before its swap
    // (composited), else its first frame presented before it shows. Replaces one up
    void open(std::vector<Ui::tab_t> rows, std::size_t selectedRow)
    {
        // Opened again over itself - a row closed from it - its rows stay scrolled as they were
        const std::size_t firstRow = m_slot.connector && m_slot.connector->hasRenderer()
                                       ? m_slot.connector->renderer().firstRow()
                                       : 0;
        close();
        if (rows.empty() || !m_main) {
            return;
        }
        const fpx_t margin  = m_resManager.layout().tabSwitcher.margin; // kept clear of the window's edges
        const fpx_t rowH    = m_resManager.layout().tabSwitcherItem.height;
        const fpx_t windowW = toCss(m_geometry.width());
        const fpx_t windowH = toCss(m_geometry.height());

        // More tabs than rows: a gutter on the right for the scrollbar, the rows' text fitted left
        // of it and of each row's close button, once - fitting them again would cut a name
        // already cut. How many rows show is the height's alone, so it is asked with no width
        const std::optional<Ui::Res::Type::bound_t> rowsPlace =
            Ui::Window::tabSwitcherBoundOf(windowW, windowH, 0.0F, margin, rowH, rows.size());
        if (!rowsPlace.has_value()) {
            return;
        }
        const bool  isScrolling = Ui::Render::rowsShownOf(rowsPlace->h, rowH) < rows.size();
        const fpx_t gutter = isScrolling ? Ui::Render::Popup::TabSwitcherRenderer::scrollGutterOf(m_resManager.layout())
                                         : 0.0F;
        const fpx_t reserved = gutter + Ui::Render::Popup::TabSwitcherRenderer::closeRoomOf(m_resManager.layout());
        m_main->window().makeCurrent();
        const fpx_t width = m_main->renderer().fitTabSwitcherRows(windowW - (margin * 2) - reserved, rows) + reserved;
        const std::optional<Ui::Res::Type::bound_t> place =
            Ui::Window::tabSwitcherBoundOf(windowW, windowH, width, margin, rowH, rows.size());
        if (!place.has_value()) {
            return;
        }
        const Ui::Res::Type::bound_t bound = { .x = toPhysEdge(place->x),
                                               .y = toPhysEdge(place->y),
                                               .w = toPhysRound(place->w),
                                               .h = toPhysRound(place->h) };

        if (!m_popups.make(
                bound,
                m_resManager.layout().tabSwitcher.border.scaled(g_config.scale),
                m_resManager.theme().tabSwitcher.bg,
                false,
                [this]() { m_underlay.recapture(m_slot); },
                m_slot)) {
            std::cerr << "[WindowManager] Failed to create the tab switcher window" << std::endl;
            return;
        }

        auto & renderer = m_popups.emplaceRenderer(m_slot, std::move(rows));
        renderer.scrollTo(firstRow);
        renderer.setSelectedRow(selectedRow);
        // Up under a pointer already there - one that just closed a row - it hovers what is now
        // under it, as a move would, with no move to wait for
        if (m_pointerX != Ui::NO_POINTER) {
            std::ignore = renderer.onMouseMove(m_pointerX - static_cast<int>(bound.x),
                                               m_pointerY - static_cast<int>(bound.y));
        }
        m_mainFrame.present(m_slot);
    }

    // The highlight on `row`, scrolled to show it
    void setSelectedRow(std::size_t row) const
    {
        if (m_slot.connector && m_slot.connector->hasRenderer()) {
            m_slot.connector->renderer().setSelectedRow(row);
            m_slot.connector->window().requestRender();
        }
    }

    // The row the pointer is over; none off it, or with no list
    [[nodiscard]] std::optional<std::size_t> hoveredRow() const
    {
        if (!m_slot.connector || !m_slot.connector->hasRenderer()) {
            return std::nullopt;
        }
        return m_slot.connector->renderer().hoveredRow();
    }

    [[nodiscard]] bool isOpen() const { return m_slot.connector != nullptr; }

    // Its scrollbar thumb held, the pointer kept by it until the button comes up
    [[nodiscard]] bool isThumbHeld() const
    {
        return m_slot.connector && m_slot.connector->hasRenderer() && m_slot.connector->renderer().isThumbHeld();
    }

    // The list's window, nullptr with none
    [[nodiscard]] const Ui::Window::Popup::PopupWindow * window() const
    {
        return m_slot.connector ? &m_slot.connector->window() : nullptr;
    }

    // Takes the list down; the main window's hover catches up with the pointer
    void close()
    {
        if (!m_slot.connector) {
            return;
        }
        if constexpr (Ui::Render::Popup::TAB_SWITCHER_DEBUG) {
            std::cout << "[TabSwitch] window taken down" << std::endl;
        }
        m_popups.close(m_slot);
        m_renderQueue.request(MAIN_WINDOW_ID);
        m_rehover();
    }

private:
    const Ui::Res::ResManager &            m_resManager;
    const std::unique_ptr<MainConnector> & m_main;
    const Geometry &                       m_geometry;
    const int &                            m_pointerX; // the WindowManager's, main-window physical px
    const int &                            m_pointerY;
    PopupWindows &                         m_popups;
    Underlay &                             m_underlay;
    MainFrame &                            m_mainFrame;
    popup_slot_t<TabSwitcherConnector> &   m_slot;
    Ui::Window::RenderQueue &              m_renderQueue;
    Ui::task_fn_t                          m_rehover; // the main window's hover caught up with the pointer, once closed
};

} // namespace Ui::Window
