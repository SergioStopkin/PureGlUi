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

// Ordered, id-keyed collection: unordered_map for O(1) identity lookup plus a
// vector that holds the visual/iteration order. The read side is reusable by
// any addressable element list; the mutate side is used by dynamic collections
// (tabs today). The caller supplies the id on add - the container never invents
// identity. "active" is intentionally not modelled here: only some collections
// have an active element, so that lives on the owner.
template <typename T>
class Registry final {
    std::unordered_map<id_t, T> m_items;
    std::vector<id_t>           m_order;

public:
    // --- read side ---
    [[nodiscard]] const T * find(id_t id) const
    {
        const auto it = m_items.find(id);
        return it == m_items.end() ? nullptr : &it->second;
    }

    [[nodiscard]] bool contains(id_t id) const { return m_items.find(id) != m_items.end(); }

    [[nodiscard]] const std::vector<id_t> & order() const { return m_order; }

    [[nodiscard]] std::size_t size() const { return m_order.size(); }

    [[nodiscard]] bool empty() const { return m_order.empty(); }

    // --- mutate side ---
    void add(id_t id, T value)
    {
        if (m_items.emplace(id, std::move(value)).second) {
            m_order.emplace_back(id);
        }
    }

    void insert(std::size_t index, id_t id, T value)
    {
        if (!m_items.emplace(id, std::move(value)).second) {
            return;
        }
        if (index > m_order.size()) {
            index = m_order.size();
        }
        m_order.emplace(m_order.begin() + static_cast<std::ptrdiff_t>(index), id);
    }

    void remove(id_t id)
    {
        if (m_items.erase(id) == 0) {
            return;
        }
        m_order.erase(std::remove(m_order.begin(), m_order.end(), id), m_order.end());
    }

    void move(id_t id, std::size_t index)
    {
        const auto it = std::find(m_order.begin(), m_order.end(), id);
        if (it == m_order.end()) {
            return;
        }
        m_order.erase(it);
        if (index > m_order.size()) {
            index = m_order.size();
        }
        m_order.emplace(m_order.begin() + static_cast<std::ptrdiff_t>(index), id);
    }

    [[nodiscard]] T * edit(id_t id)
    {
        const auto it = m_items.find(id);
        return it == m_items.end() ? nullptr : &it->second;
    }
};

} // namespace Ui
