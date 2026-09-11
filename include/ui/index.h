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

#include <utility>
#include <vector>

namespace Ui {

// Group-by: key -> many values, for the acceleration structures that would
// otherwise be a scan inside a loop. Built once from a flat collection, then
// read O(1) per key - a parent-to-children index over a tree being the case
// this exists for.
//
// Composed over Registry rather than reimplementing map + order, so the two
// stay one storage strategy. Registry means "one value per key, in order";
// this means "many values per key". Keeping them separate keeps Registry's
// meaning intact.
//
// Insertion order is preserved within a group, which is what a tree walk
// needs: children come out in the order the source listed them.
template <typename Key, typename T>
class Index final {
    Registry<Key, std::vector<T>> m_groups;

public:
    void add(const Key & key, T value) { m_groups.findOrAdd(key).emplace_back(std::move(value)); }

    // An absent key yields an empty group, not null, so callers walk the
    // result without first asking whether the key exists.
    [[nodiscard]] const std::vector<T> & group(const Key & key) const
    {
        static const std::vector<T> EMPTY;
        const std::vector<T> *      found = m_groups.find(key);
        return (found != nullptr) ? *found : EMPTY;
    }

    [[nodiscard]] bool contains(const Key & key) const { return m_groups.contains(key); }

    // Number of distinct keys, not of values
    [[nodiscard]] std::size_t size() const { return m_groups.size(); }

    [[nodiscard]] bool empty() const { return m_groups.empty(); }

    // Keys in first-seen order
    [[nodiscard]] const std::vector<Key> & keys() const { return m_groups.order(); }
};

} // namespace Ui
