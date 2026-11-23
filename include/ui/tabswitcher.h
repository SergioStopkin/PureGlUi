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
#include "ui/render/popup/tabswitcherrenderer.h"
#include "ui/res/resmanager.h"
#include "ui/tab.h"
#include "ui/tabbar.h"
#include "ui/tabswitch.h"
#include "ui/type.h"
#include "ui/window/event.h"
#include "ui/window/windowmanager.h"

#include <cstddef>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

namespace Ui {

// The Ctrl+Tab list: its Ui::TabSwitch and the WindowManager's window showing it, kept in
// step. Which tab it switches to is handed back - activating one is the shell's
class TabSwitcher final : private Common::NonCopyable {
public:
    TabSwitcher(const Ui::Res::ResManager & resManager, Ui::Window::WindowManager & windowManager)
        : m_resManager(resManager)
        , m_windowManager(windowManager)
    {
    }

    // The list while Ctrl is held, up or not yet: what OpenTabSwitcher opens
    [[nodiscard]] Ui::TabSwitch & tabSwitch() { return m_tabSwitch; }

    [[nodiscard]] bool isOpen() const { return m_tabSwitch.isOpen(); }

    // The highlight `delta` rows on, and the window showing it
    void cycle(int delta)
    {
        m_tabSwitch.cycle(delta);
        m_windowManager.tabSwitcher().setSelectedRow(m_tabSwitch.selectedRow());
    }

    // The highlight on the row the pointer is over, when it is over one - the one Ctrl going
    // up takes: on each move, on letting go of its thumb, and on a wheel turn that moved the
    // rows under the pointer. Never on the keys' steps, which pick the row themselves
    void followHover()
    {
        if (!m_tabSwitch.isShown()) {
            return;
        }
        const std::optional<std::size_t> row = m_windowManager.tabSwitcher().hoveredRow();
        if (row.has_value() && row.value() != m_tabSwitch.selectedRow()) {
            m_tabSwitch.select(row.value());
            m_windowManager.tabSwitcher().setSelectedRow(row.value());
        }
    }

    // Up once Ctrl has been held past its delay: a held key sends no event to show it on
    void showIfDue()
    {
        if (m_tabSwitch.isDue(Common::Time::now())) {
            show();
        }
    }

    // After a tab closed from it: a row per tab the host has left, the highlight on the row
    // it stood on, opened again over itself; taken down with none left
    void refresh()
    {
        if (!m_tabSwitch.isShown()) {
            return;
        }
        m_tabSwitch.setIds(m_resManager.tabBar().recentIds());
        if constexpr (Ui::Render::Popup::TAB_SWITCHER_DEBUG) {
            std::cout << "[TabSwitch] a tab closed from it: " << m_tabSwitch.ids().size() << " left, row "
                      << m_tabSwitch.selectedRow() << " highlighted" << std::endl;
        }
        if (m_tabSwitch.isShown()) {
            show();
        } else {
            m_windowManager.tabSwitcher().close();
        }
    }

    // The list taken down, and the tab it switches to, as a click on its tab does -
    // INVALID_ID for the active one, or one closed since it opened
    [[nodiscard]] id_t takeChosenId()
    {
        const id_t selectedId = m_tabSwitch.selectedId();
        if constexpr (Ui::Render::Popup::TAB_SWITCHER_DEBUG) {
            std::cout << "[TabSwitch] switching to row " << m_tabSwitch.selectedRow() << ", tab " << selectedId
                      << std::endl;
        }
        cancel();
        const Ui::TabBar & tabBar = m_resManager.tabBar();
        return selectedId != tabBar.activeId() && tabBar.contains(selectedId) ? selectedId : Ui::INVALID_ID;
    }

    // The list taken down, unswitched
    void cancel()
    {
        if (!m_tabSwitch.isOpen()) {
            return;
        }
        if constexpr (Ui::Render::Popup::TAB_SWITCHER_DEBUG) {
            const Ui::Window::Event & event = m_windowManager.currentEvent();
            std::cout << "[TabSwitch] closed on event type " << static_cast<int>(event.type)
                      << " list=" << event.isPopupEvent << " child=" << event.childWindowId << " at " << event.mouse.x
                      << "," << event.mouse.y << " key='" << event.toShortcutString() << "'" << std::endl;
        }
        m_tabSwitch.close();
        m_windowManager.tabSwitcher().close();
    }

private:
    const Ui::Res::ResManager & m_resManager;
    Ui::Window::WindowManager & m_windowManager;
    Ui::TabSwitch               m_tabSwitch; // up or not yet

    // Up: a row per tab, as TabBar holds them
    void show()
    {
        m_tabSwitch.show();
        std::vector<Ui::tab_t> rows;
        rows.reserve(m_tabSwitch.ids().size());
        for (const id_t id : m_tabSwitch.ids()) {
            m_resManager.tabBar().visit(id, [&rows](const Ui::tab_t & tab) { rows.emplace_back(tab); });
        }
        m_windowManager.tabSwitcher().open(std::move(rows), m_tabSwitch.selectedRow());
    }
};

} // namespace Ui
