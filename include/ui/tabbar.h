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

#include "ui/registry.h"
#include "ui/type.h"
#include "ui/type/tab.h"

#include <utility>
#include <vector>

namespace Ui {

// The chrome's tab model: an ordered set of tab_t view models plus the scroll
// offset (which tab is leftmost-visible). The host projects its authoritative
// workspace state into this with setTabs() on the main thread; the renderer
// reads it. Active-ness rides inside each tab_t (tab_t::isActive), so Registry
// stays active-agnostic. Scroll is the one piece of authoritative state owned
// here - it is pure chrome concern with no domain meaning.
class TabBar final {
    Registry<Type::tab_t> m_tabs;
    std::size_t           m_scrollOffset = 0;

public:
    // --- read side (renderer) ---
    [[nodiscard]] const std::vector<id_t> & order() const { return m_tabs.order(); }

    [[nodiscard]] const Type::tab_t * find(id_t id) const { return m_tabs.find(id); }

    [[nodiscard]] std::size_t scrollOffset() const { return m_scrollOffset; }

    [[nodiscard]] std::size_t count() const { return m_tabs.size(); }

    [[nodiscard]] bool empty() const { return m_tabs.empty(); }

    // --- write side (host, main thread) ---
    // Replace the projected tabs in one shot (order = vector order). Scroll is
    // preserved and clamped to the new count, matching the prior in-place clamp.
    void setTabs(std::vector<Type::tab_t> tabs)
    {
        m_tabs = {};
        for (auto & tab : tabs) {
            const id_t id = tab.id;
            m_tabs.add(id, std::move(tab));
        }
        if (m_scrollOffset >= m_tabs.size()) {
            m_scrollOffset = m_tabs.empty() ? 0 : m_tabs.size() - 1;
        }
    }

    // In-place refresh of the volatile loading fields. Cheap path for progress
    // ticks - no rebuild, no allocation. Structure/label/active are untouched.
    void setLoading(id_t id, bool isLoading, int progress)
    {
        if (Type::tab_t * tab = m_tabs.edit(id)) {
            tab->isLoading = isLoading;
            tab->progress  = progress;
        }
    }

    void scrollLeft()
    {
        if (m_scrollOffset > 0) {
            --m_scrollOffset;
        }
    }

    void scrollRight()
    {
        if (m_scrollOffset + 1 < m_tabs.size()) {
            ++m_scrollOffset;
        }
    }
};

} // namespace Ui
