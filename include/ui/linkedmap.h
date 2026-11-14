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

#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <optional>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace Ui {

// Keyed collection in an order of its own (Java's LinkedHashMap): a hash map over a
// doubly linked list, so a lookup, an add, a remove, a move, a neighbour and an end
// are each O(1)
//
// A place is named by a neighbour's key, never an index: an index is O(n) here, and
// no structure keeps both O(1) - list indexing is Omega(log n / log log n) (Fredman-Saks)
//
// A value is reached through visit, never a pointer: the reference lives only for the
// call, so it cannot outlast a remove. A key comes back as a copy, none when there is none
//
// The caller supplies the key, and an active element lives on its owner, since only
// some collections have one. Key is a parameter because a numeric id_t is reassigned
// whenever resources reload while a string key_t survives that. One value per key;
// Ui::Index holds many
template <typename Key, typename T>
class LinkedMap final {
    using entries_t = std::list<std::pair<const Key, T>>;

    entries_t m_entries; // in order; a node never moves, so a value's address holds
    std::unordered_map<Key, typename entries_t::iterator> m_nodes; // each key's node in m_entries

public:
    LinkedMap() = default;

    // A copy's lookups point at its own nodes, never the source's
    LinkedMap(const LinkedMap & other)
    {
        for (const auto & [key, value] : other.m_entries) {
            m_nodes.emplace(key, m_entries.emplace(m_entries.end(), key, value));
        }
    }

    // A move hands the nodes over, so the lookups stay valid
    LinkedMap(LinkedMap && other) noexcept = default;

    LinkedMap & operator=(const LinkedMap & other)
    {
        if (this != &other) {
            *this = LinkedMap(other);
        }
        return *this;
    }

    LinkedMap & operator=(LinkedMap && other) noexcept = default;

    // --- read side ---
    // Calls fn with key's value when it is held; whether it was
    template <typename Fn>
        requires std::invocable<Fn, const T &>
    bool visit(const Key & key, Fn && fn) const
    {
        const auto it = m_nodes.find(key);
        if (it == m_nodes.end()) {
            return false;
        }
        std::invoke(std::forward<Fn>(fn), std::as_const(it->second->second));
        return true;
    }

    [[nodiscard]] bool contains(const Key & key) const { return m_nodes.contains(key); }

    // In order, as a view: a walk copies nothing
    [[nodiscard]] auto keys() const { return m_entries | std::views::keys; }

    // The key right after key; none for the last or an absent one
    [[nodiscard]] std::optional<Key> keyAfter(const Key & key) const
    {
        const auto it = m_nodes.find(key);
        if (it == m_nodes.end()) {
            return std::nullopt;
        }
        const auto after = std::next(it->second);
        if (after == m_entries.end()) {
            return std::nullopt;
        }
        return after->first;
    }

    // The key right before key; none for the first or an absent one
    [[nodiscard]] std::optional<Key> keyBefore(const Key & key) const
    {
        const auto it = m_nodes.find(key);
        if (it == m_nodes.end() || it->second == m_entries.begin()) {
            return std::nullopt;
        }
        return std::prev(it->second)->first;
    }

    // None while empty
    [[nodiscard]] std::optional<Key> firstKey() const
    {
        if (m_entries.empty()) {
            return std::nullopt;
        }
        return m_entries.front().first;
    }

    // None while empty
    [[nodiscard]] std::optional<Key> lastKey() const
    {
        if (m_entries.empty()) {
            return std::nullopt;
        }
        return m_entries.back().first;
    }

    [[nodiscard]] std::size_t size() const { return m_entries.size(); }

    [[nodiscard]] bool empty() const { return m_entries.empty(); }

    // --- mutate side ---
    // Calls fn with key's value to change in place when it is held; whether it was
    template <typename Fn>
        requires std::invocable<Fn, T &>
    bool visit(const Key & key, Fn && fn)
    {
        const auto it = m_nodes.find(key);
        if (it == m_nodes.end()) {
            return false;
        }
        std::invoke(std::forward<Fn>(fn), it->second->second);
        return true;
    }

    // At the end; nothing for a key already held. Moved in, never by value: MSVC passes no
    // over-aligned value (C2719)
    void add(const Key & key, T && value)
    {
        const auto [it, isAdded] = m_nodes.try_emplace(key);
        if (isAdded) {
            it->second = m_entries.emplace(m_entries.end(), key, std::move(value));
        }
    }

    void remove(const Key & key)
    {
        const auto it = m_nodes.find(key);
        if (it == m_nodes.end()) {
            return;
        }
        m_entries.erase(it->second);
        m_nodes.erase(it);
    }

    // Right before anchor; nothing while either is absent
    void moveBefore(const Key & key, const Key & anchor)
    {
        const auto moved  = m_nodes.find(key);
        const auto before = m_nodes.find(anchor);
        if (moved == m_nodes.end() || before == m_nodes.end()) {
            return;
        }
        m_entries.splice(before->second, m_entries, moved->second);
    }

    void moveToBack(const Key & key)
    {
        const auto it = m_nodes.find(key);
        if (it != m_nodes.end()) {
            m_entries.splice(m_entries.end(), m_entries, it->second);
        }
    }

    // Existing value, or a default-constructed one at the end - built in its node,
    // so a value that cannot move (an atomic) is held directly. The seam Index
    // needs: appending to a group requires the group to exist
    [[nodiscard]] T & findOrAdd(const Key & key)
    {
        const auto [it, isAdded] = m_nodes.try_emplace(key);
        if (isAdded) {
            it->second = m_entries.emplace(m_entries.end(),
                                           std::piecewise_construct,
                                           std::forward_as_tuple(key),
                                           std::forward_as_tuple());
        }
        return it->second->second;
    }
};

} // namespace Ui
