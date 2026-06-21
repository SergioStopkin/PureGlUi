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

#include "common/sanitize.h"
#include "nlohmann/json.hpp"
#include "ui/res/dock/state.h"
#include "ui/res/store/layoutstore.h"
#include "ui/type.h"

#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace Ui::Res::Store {

// Session-persisted per-dock state (collapsed width + remembered width), keyed
// by dock name. Separate from dock CONFIG (which lives in layout_t, loaded by
// ResManager::loadDocks); this store only holds the user's runtime prefs and
// (de)serializes them to the host session JSON. The dock CONFIG list (read for
// orphan-filtering on write) comes from the injected LayoutStore.
//
// setDockState is const + mutable storage so a subsystem holding a
// `const ResManager&` (e.g. WindowManager) can commit dock prefs; the facade
// fires the persist hook around it.
class DockStore final {
    const Store::LayoutStore &                                           m_layoutStore;
    mutable std::unordered_map<std::string, Ui::Res::Dock::dock_state_t> m_dockStates;

public:
    // Dock name = persistence key. Cap well below Sanitize's 4 KB default so a
    // malformed file can't blow up unordered_map buckets with huge keys.
    static constexpr std::size_t MAX_DOCK_NAME_LENGTH = 64;

    explicit DockStore(const Store::LayoutStore & layoutStore)
        : m_layoutStore(layoutStore)
    {
    }

    // Reject malformed numeric values from session.json: negative, NaN, infinity
    // all become 0 (collapsed). The viewport-aware max is applied later by
    // WindowManager once the DockColumn instances exist.
    static fpx_t sanitizeDockWidth(double value)
    {
        if (!std::isfinite(value) || value < 0.0) {
            return 0.0F;
        }
        return static_cast<fpx_t>(value);
    }

    // Per-dock session state. Returns a default-constructed state (collapsed,
    // width=0) when absent - matches the "new dock starts collapsed" rule.
    [[nodiscard]] Ui::Res::Dock::dock_state_t dockState(const std::string & name) const
    {
        auto it = m_dockStates.find(name);
        return it == m_dockStates.end() ? Ui::Res::Dock::dock_state_t {} : it->second;
    }

    // Commit a dock's state. The facade fires the persist hook around this.
    // NOT thread-safe; every caller is on the UI thread.
    void setDockState(const std::string & name, const Ui::Res::Dock::dock_state_t & state) const
    {
        m_dockStates[name] = state;
    }

    // Serialize per-dock state into the host's session JSON. Only docks that both
    // exist in the current layout and carry a stored state are written; orphans
    // for removed docks are dropped here, by design.
    void writeDockJson(nlohmann::json & j) const
    {
        nlohmann::json dockArray = nlohmann::json::array();
        for (const auto & cfg : m_layoutStore.layout().docks) {
            auto it = m_dockStates.find(cfg.name);
            if (it == m_dockStates.end()) {
                continue;
            }
            nlohmann::json entry;
            entry["name"]    = cfg.name;
            entry["width"]   = it->second.width;
            entry["memoryX"] = it->second.memoryX;
            dockArray.emplace_back(std::move(entry));
        }
        if (!dockArray.empty()) {
            j["dock"] = std::move(dockArray);
        }
    }

    // Restore per-dock state from the host's session JSON. DockColumn instances
    // look themselves up by name when WindowManager builds them in loadAll().
    void readDockJson(const nlohmann::json & j)
    {
        m_dockStates.clear();
        if (!j.contains("dock") || !j["dock"].is_array()) {
            return;
        }
        for (const auto & entry : j["dock"]) {
            if (!entry.is_object()) {
                continue;
            }
            const std::string name = Common::Sanitize::string(entry.value("name", std::string {}),
                                                              "dock.name",
                                                              MAX_DOCK_NAME_LENGTH);
            if (name.empty()) {
                continue;
            }
            Ui::Res::Dock::dock_state_t state;
            state.width   = sanitizeDockWidth(entry.value("width", 0.0));
            state.memoryX = sanitizeDockWidth(entry.value("memoryX", 0.0));
            m_dockStates.emplace(name, state);
        }
    }
};

} // namespace Ui::Res::Store
