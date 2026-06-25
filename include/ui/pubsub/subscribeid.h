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
#include "ui/window/event.h"

namespace Ui::PubSub {

// Element ID ranges are a UI-framework concern - see Ui::ElementId
// (ui/elementid.h). Documented here only for the full id-space picture.
//
// Subscribe source ID ranges:
//   1-99:        window IDs
//   100-199:     workspace render IDs (WS_BASE + wsId)
//   200-299:     popup subscriber IDs (POPUP_BASE + popupId)
//   300-399:     dock source IDs      (DOCK_BASE + dockIndex)
//   10000+:      OS event source IDs  (EVENT_BASE + EventType)
//   20000+:      action source IDs    (ACTION_BASE + action ordinal)
//
// Subscribe subscriber ID ranges:
//   1-99:        window IDs
//   100-199:     workspace IDs
//   200-299:     popup IDs
//   10000:       app subscriber ID

enum class SourceId : id_t {
    MainWindow = 1,
    WsBase     = 100,
    PopupBase  = 200,
    DockBase   = 300,
    EventBase  = 10000,
    ActionBase = 20000,
};

enum class SubscriberId : id_t {
    App = 10000,
};

// Source ID helpers

constexpr id_t sourceId(SourceId base) { return static_cast<id_t>(base); }

constexpr id_t sourceId(SourceId base, id_t offset) { return static_cast<id_t>(base) + offset; }

inline id_t eventSourceId(Ui::Window::EventType type) { return sourceId(SourceId::EventBase, static_cast<id_t>(type)); }

constexpr id_t wsSourceId(id_t wsId) { return sourceId(SourceId::WsBase, wsId); }

constexpr id_t dockSourceId(id_t dockIndex) { return sourceId(SourceId::DockBase, dockIndex); }

// Subscriber ID helpers

constexpr id_t subscriberId(SubscriberId id) { return static_cast<id_t>(id); }

} // namespace Ui::PubSub
