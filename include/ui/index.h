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

#include <concepts>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace Ui {

// Group-by: key -> many values, for the acceleration structures that would
// otherwise be a scan inside a loop. Built once from a flat collection, then
// read O(1) per key - a parent-to-children index over a tree being the case
// this exists for.
//
// Composed over LinkedMap rather than reimplementing map + order, so the two
// stay one storage strategy. LinkedMap means "one value per key, in order";
// this means "many values per key". Keeping them separate keeps LinkedMap's
// meaning intact.
//
// Insertion order is preserved within a group, which is what a tree walk
// needs: children come out in the order the source listed them.
template <typename Key, typename T>
class Index final {
    LinkedMap<Key, std::vector<T>> m_groups;

public:
    // Moved in, as LinkedMap::add
    void add(const Key & key, T && value)
    {
        m_groups.visitOrAdd(key, [&value](std::vector<T> & group) { group.emplace_back(std::move(value)); });
    }

    // Calls fn with each value under key, in the order added - none for a key not held, so a
    // caller walks a group without asking first; whether it was held. As LinkedMap::visit, no
    // reference outlives the call
    template <typename Fn>
        requires std::invocable<const Fn &, const T &>
    bool visit(const Key & key, const Fn & fn) const
    {
        return m_groups.visit(key, [&fn](const std::vector<T> & group) {
            for (const T & value : group) {
                std::invoke(fn, value);
            }
        });
    }

    [[nodiscard]] bool contains(const Key & key) const { return m_groups.contains(key); }

    // Number of distinct keys, not of values
    [[nodiscard]] std::size_t size() const { return m_groups.size(); }

    [[nodiscard]] bool empty() const { return m_groups.empty(); }

    // Keys in first-seen order
    [[nodiscard]] auto keys() const { return m_groups.keys(); }
};

} // namespace Ui
