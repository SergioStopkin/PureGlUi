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
#include <functional>
#include <initializer_list>
#include <vector>

namespace Ui::PubSub {

class Subscribe final {
    struct alignas(32) entry_t final {
        id_t                  source {};
        id_t                  subscriber {};
        std::function<void()> callback;
    };

    std::vector<entry_t>               m_entries;
    std::vector<std::function<void()>> m_deferred;

public:
    Subscribe()  = default;
    ~Subscribe() = default;

    Subscribe(const Subscribe &)             = delete;
    Subscribe(Subscribe &&)                  = delete;
    Subscribe & operator=(const Subscribe &) = delete;
    Subscribe & operator=(Subscribe &&)      = delete;

    void add(id_t source, id_t subscriber, std::function<void()> callback)
    {
        m_entries.emplace_back(entry_t { source, subscriber, std::move(callback) });
    }

    void add(std::initializer_list<id_t> sources, id_t subscriber, std::function<void()> callback)
    {
        for (const id_t source : sources) {
            m_entries.emplace_back(entry_t { source, subscriber, callback });
        }
    }

    void remove(id_t subscriber)
    {
        m_entries.erase(std::remove_if(m_entries.begin(),
                                       m_entries.end(),
                                       [subscriber](const entry_t & entry) { return entry.subscriber == subscriber; }),
                        m_entries.end());
    }

    void notify(id_t source)
    {
        // Snapshot: callbacks may add/remove entries during iteration
        const auto snapshot = m_entries;
        for (const auto & entry : snapshot) {
            if (entry.source == source) {
                if (entry.callback) {
                    entry.callback();
                }
            }
        }
    }

    void defer(std::function<void()> action) { m_deferred.emplace_back(std::move(action)); }

    void drainDeferred()
    {
        while (!m_deferred.empty()) {
            auto actions = std::move(m_deferred);
            for (auto & action : actions) {
                if (action) {
                    action();
                }
            }
        }
    }
};

} // namespace Ui::PubSub
