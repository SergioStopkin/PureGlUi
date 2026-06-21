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

#include "ui/res/dock/anchor.h"
#include "ui/type.h"

#include <string>

namespace Ui::Res::Dock {

// One dock definition loaded from res/dock/*.json (one file per dock).
// Describes where a dock anchors and what width to use when the user
// expands it from a fresh (no-session-entry) state. Whether the dock is
// currently expanded is *not* configured here: first launch is always
// collapsed (dock_state_t default), and any later state persists in
// session.json under the dock's name.
struct alignas(64) dock_config_t final {
    std::string name;            // stable identity, used as persistence key
    DockAnchor  anchor {};       // which screen edge it docks against
    int         order = 1;       // 1 = innermost (next to viewport), 2 = beyond it, ...
    fpx_t       defaultWidth {}; // width applied the first time the dock is expanded

    bool operator==(const dock_config_t &) const = default;
};

} // namespace Ui::Res::Dock
