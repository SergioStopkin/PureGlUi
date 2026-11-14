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

#include "common/time.h"
#include "ui/type.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace Ui {

// The Ctrl+Tab list while Ctrl is held: the tabs most recent first, the one
// highlighted, and whether it is up. It goes up only once Ctrl has been held past its
// delay, so a quick tap switches with no list; the window is WindowManager's, this is
// what it shows, kept apart so it is the same whether shown or not
class TabSwitch final {
    std::vector<id_t>     m_ids;          // most recent first, the active one first; empty while closed
    std::size_t           m_selected = 0; // the highlighted one
    Common::Time::point_t m_due;          // when the list goes up, Ctrl still held
    bool                  m_isShown = false;

public:
    // Opens on the tab `delta` rows from the active one: 1 the one viewed before it,
    // -1 the least recent. Nothing for no tab
    void open(std::vector<id_t> recentIds, int delta, Common::Time::point_t due)
    {
        m_ids      = std::move(recentIds);
        m_selected = 0;
        m_due      = due;
        m_isShown  = false;
        cycle(delta);
    }

    void close()
    {
        m_ids.clear();
        m_isShown = false;
    }

    [[nodiscard]] bool isOpen() const { return !m_ids.empty(); }

    [[nodiscard]] bool isShown() const { return m_isShown; }

    // Open, not up yet, and its delay run out at `now`
    [[nodiscard]] bool isDue(Common::Time::point_t now) const { return isOpen() && !m_isShown && now >= m_due; }

    void setShown() { m_isShown = isOpen(); }

    // The highlight `delta` rows on, wrapping at either end
    void cycle(int delta)
    {
        if (m_ids.empty()) {
            return;
        }
        const auto count = static_cast<std::ptrdiff_t>(m_ids.size());
        const auto moved = (static_cast<std::ptrdiff_t>(m_selected) + delta) % count;
        m_selected       = static_cast<std::size_t>(moved < 0 ? moved + count : moved);
    }

    // The highlight on `row` - the one the pointer is over; nothing past the last
    void select(std::size_t row)
    {
        if (row < m_ids.size()) {
            m_selected = row;
        }
    }

    [[nodiscard]] std::size_t selectedRow() const { return m_selected; }

    // INVALID_ID while closed
    [[nodiscard]] id_t selectedId() const { return isOpen() ? m_ids.at(m_selected) : INVALID_ID; }

    [[nodiscard]] const std::vector<id_t> & ids() const { return m_ids; }
};

} // namespace Ui
