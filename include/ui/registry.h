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

#include "ui/type.h"

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui {

// Ordered, keyed collection: unordered_map for O(1) identity lookup plus a
// vector that holds the visual/iteration order. The read side is reusable by
// any addressable element list; the mutate side is used by dynamic collections
// (tabs today). The caller supplies the key on add - the container never
// invents identity. "active" is intentionally not modelled here: only some
// collections have an active element, so that lives on the owner.
//
// Key is a parameter because the two identities in this codebase serve
// different jobs and both need indexing: a numeric id_t is fast but is
// reassigned whenever resources reload, while a string key_t survives that
// (see ResManager::findMenuItemByKey). Anything hashable works.
//
// One value per key. For key -> many values use Ui::Index, which composes over
// this rather than duplicating the storage.
template <typename Key, typename T>
class Registry final {
    std::unordered_map<Key, T> m_items;
    std::vector<Key>           m_order;

public:
    // --- read side ---
    [[nodiscard]] const T * find(const Key & key) const
    {
        const auto it = m_items.find(key);
        return it == m_items.end() ? nullptr : &it->second;
    }

    [[nodiscard]] bool contains(const Key & key) const { return m_items.find(key) != m_items.end(); }

    [[nodiscard]] const std::vector<Key> & order() const { return m_order; }

    [[nodiscard]] std::size_t size() const { return m_order.size(); }

    [[nodiscard]] bool empty() const { return m_order.empty(); }

    // --- mutate side ---
    void add(const Key & key, T value)
    {
        if (m_items.emplace(key, std::move(value)).second) {
            m_order.emplace_back(key);
        }
    }

    void insert(std::size_t index, const Key & key, T value)
    {
        if (!m_items.emplace(key, std::move(value)).second) {
            return;
        }
        if (index > m_order.size()) {
            index = m_order.size();
        }
        m_order.emplace(m_order.begin() + static_cast<std::ptrdiff_t>(index), key);
    }

    void remove(const Key & key)
    {
        if (m_items.erase(key) == 0) {
            return;
        }
        m_order.erase(std::remove(m_order.begin(), m_order.end(), key), m_order.end());
    }

    void move(const Key & key, std::size_t index)
    {
        const auto it = std::find(m_order.begin(), m_order.end(), key);
        if (it == m_order.end()) {
            return;
        }
        m_order.erase(it);
        if (index > m_order.size()) {
            index = m_order.size();
        }
        m_order.emplace(m_order.begin() + static_cast<std::ptrdiff_t>(index), key);
    }

    [[nodiscard]] T * edit(const Key & key)
    {
        const auto it = m_items.find(key);
        return it == m_items.end() ? nullptr : &it->second;
    }

    // Existing value, or a default-constructed one joined to the order. The
    // seam Index needs: appending to a group requires the group to exist.
    [[nodiscard]] T & findOrAdd(const Key & key)
    {
        const auto [it, inserted] = m_items.try_emplace(key);
        if (inserted) {
            m_order.emplace_back(key);
        }
        return it->second;
    }
};

} // namespace Ui
