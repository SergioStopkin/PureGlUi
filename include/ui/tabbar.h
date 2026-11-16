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

#include "ui/linkedmap.h"
#include "ui/revealedoffsetof.h"
#include "ui/tab.h"
#include "ui/type.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace Ui {

// The chrome's tab model: an ordered set of tab_t view models plus the scroll
// offset (which tab is leftmost-visible). The host projects its authoritative
// workspace state into this with setTabs() on the main thread; the renderer
// reads it. The active tab is the one the host projects active; LinkedMap stays
// active-agnostic. Scroll is the one piece of authoritative state owned here -
// it is pure chrome concern with no domain meaning.
class TabBar final {
    LinkedMap<id_t, tab_t> m_tabs;
    std::vector<id_t>      m_recentIds;                 // most recent first, the Ctrl+Tab list's order
    id_t                   m_activeId     = INVALID_ID; // the first tab projected active
    std::size_t            m_scrollOffset = 0;

    // The last first tab that leaves a strip showing `shown` at a time full; 0 while it shows all
    [[nodiscard]] std::size_t lastFirstShownOf(std::size_t shown) const
    {
        return m_tabs.size() > shown ? m_tabs.size() - shown : 0;
    }

public:
    // --- read side (renderer) ---
    [[nodiscard]] auto order() const { return m_tabs.keys(); }

    // The tabs by when last active, the most recent - the active one - first. Rebuilt
    // whole with every projection and read by position, so a vector
    [[nodiscard]] const std::vector<id_t> & recentIds() const { return m_recentIds; }

    // Calls fn with the tab when it is held; whether it was
    template <typename Fn>
        requires std::invocable<Fn, const tab_t &>
    bool visit(id_t id, Fn && fn) const
    {
        return m_tabs.visit(id, std::forward<Fn>(fn));
    }

    [[nodiscard]] bool contains(id_t id) const { return m_tabs.contains(id); }

    // INVALID_ID while no tab is
    [[nodiscard]] id_t activeId() const { return m_activeId; }

    // The tab right of id, the first after the last; INVALID_ID for an id it does not hold
    [[nodiscard]] id_t idAfter(id_t id) const
    {
        if (!m_tabs.contains(id)) {
            return INVALID_ID;
        }
        return m_tabs.keyAfter(id).value_or(m_tabs.firstKey().value_or(INVALID_ID));
    }

    // The tab left of id, the last before the first; INVALID_ID for an id it does not hold
    [[nodiscard]] id_t idBefore(id_t id) const
    {
        if (!m_tabs.contains(id)) {
            return INVALID_ID;
        }
        return m_tabs.keyBefore(id).value_or(m_tabs.lastKey().value_or(INVALID_ID));
    }

    // The tab right of id on the strip; none for the last - no wrapping, unlike idAfter
    [[nodiscard]] std::optional<id_t> rightIdOf(id_t id) const { return m_tabs.keyAfter(id); }

    // The tab left of id on the strip; none for the first - no wrapping, unlike idBefore
    [[nodiscard]] std::optional<id_t> leftIdOf(id_t id) const { return m_tabs.keyBefore(id); }

    [[nodiscard]] std::size_t scrollOffset() const { return m_scrollOffset; }

    [[nodiscard]] std::size_t count() const { return m_tabs.size(); }

    [[nodiscard]] bool empty() const { return m_tabs.empty(); }

    // --- write side (host, main thread) ---
    // Replace the projected tabs in one shot (order = vector order), a repeated id
    // keeping its first, with their ids by when last active, the most recent first. A
    // host that keeps no such order gets the strip's. Scroll is preserved and clamped
    // to the new count
    void setTabs(std::vector<tab_t> tabs, std::vector<id_t> recentIds = {})
    {
        m_tabs.clear();
        m_activeId = INVALID_ID;
        for (auto & tab : tabs) {
            const id_t id = tab.id;
            if (tab.isActive && m_activeId == INVALID_ID && !m_tabs.contains(id)) {
                m_activeId = id;
            }
            m_tabs.add(id, std::move(tab));
        }
        if (recentIds.empty()) {
            const auto ids = m_tabs.keys();
            recentIds.assign(ids.begin(), ids.end());
        }
        // Tabs it holds alone, so a row of the Ctrl+Tab list always names one; the active one
        // first, the list counting its rows from it
        std::erase_if(recentIds, [this](id_t id) { return !m_tabs.contains(id); });
        if (const auto active = std::ranges::find(recentIds, m_activeId); active != recentIds.end()) {
            std::rotate(recentIds.begin(), active, std::next(active));
        }
        m_recentIds = std::move(recentIds);
        if (m_scrollOffset >= m_tabs.size()) {
            m_scrollOffset = m_tabs.empty() ? 0 : m_tabs.size() - 1;
        }
    }

    // In-place refresh of the volatile loading fields. Cheap path for progress
    // ticks - no rebuild, no allocation. Structure/label/active are untouched.
    void setLoading(id_t id, bool isLoading, int progress)
    {
        m_tabs.visit(id, [isLoading, progress](tab_t & tab) {
            tab.isLoading = isLoading;
            tab.progress  = progress;
        });
    }

    // The first tab a strip showing `shown` at a time (Ui::Render::tabsShownOf) draws: the
    // offset, kept among those that leave it full - a tab closed, or the window widened, can
    // leave the offset past the last of them, and the strip then shows its last `shown`
    [[nodiscard]] std::size_t firstShown(std::size_t shown) const
    {
        return std::min(m_scrollOffset, lastFirstShownOf(shown));
    }

    // The arrows: a tab on either way from the first shown, never past where the strip is full
    void scrollLeft(std::size_t shown)
    {
        m_scrollOffset = firstShown(shown);
        if (m_scrollOffset > 0) {
            --m_scrollOffset;
        }
    }

    void scrollRight(std::size_t shown) { m_scrollOffset = std::min(firstShown(shown) + 1, lastFirstShownOf(shown)); }

    // Scroll the least that brings tab `index` in among the `shown` from the first shown: a tab
    // activated other than by a click on it
    void reveal(std::size_t index, std::size_t shown)
    {
        m_scrollOffset = Ui::revealedOffsetOf(firstShown(shown), index, shown);
    }
};

} // namespace Ui
