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
#include "ui/pubsub/subscription.h"
#include "ui/type.h"

#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

namespace Ui::PubSub {

class Subscribe final : private Common::NonCopyable {
    std::vector<Ui::PubSub::subscription_t> m_entries;
    std::vector<Ui::task_fn_t>              m_deferred;

public:
    Subscribe()  = default;
    ~Subscribe() = default;

    void add(id_t source, id_t subscriber, Ui::task_fn_t callback)
    {
        m_entries.emplace_back(
        Ui::PubSub::subscription_t { .source = source, .subscriber = subscriber, .callback = std::move(callback) });
    }

    void add(std::initializer_list<id_t> sources, id_t subscriber, const Ui::task_fn_t & callback)
    {
        for (const id_t source : sources) {
            m_entries.emplace_back(
            Ui::PubSub::subscription_t { .source = source, .subscriber = subscriber, .callback = callback });
        }
    }

    void remove(id_t subscriber)
    {
        std::erase_if(m_entries, [subscriber](const Ui::PubSub::subscription_t & entry) {
            return entry.subscriber == subscriber;
        });
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

    void defer(Ui::task_fn_t action) { m_deferred.emplace_back(std::move(action)); }

    void drainDeferred()
    {
        while (!m_deferred.empty()) {
            auto actions = std::move(m_deferred);
            // A moved-from vector is valid-but-unspecified; reset to a known-empty
            // state so the loop condition and any re-entrant defer() are well-defined.
            m_deferred.clear();
            for (auto & action : actions) {
                if (action) {
                    action();
                }
            }
        }
    }
};

} // namespace Ui::PubSub
