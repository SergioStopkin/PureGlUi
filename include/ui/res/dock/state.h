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

namespace Ui::Res::Dock {

// Runtime mutable state of one dock. Session-persisted so the user sees the
// same layout between runs. Static config (anchor, default width, name)
// lives in dock_config_t.
//
// width   = currently visible content width. 0 means collapsed (only the
//           grip shows). Updated 1:1 with the cursor during a drag, so
//           starting from 0 grows smoothly from the grip outward.
// memoryX = restore size for double-click on the grip. Tracks the most
//           recent non-zero release width so a drag-to-zero (collapse)
//           preserves "what to come back to". Falls back to
//           dock_config_t::defaultWidth on first ever expand.
struct alignas(8) dock_state_t final {
    fpx_t width {};
    fpx_t memoryX {};

    bool operator==(const dock_state_t &) const = default;
};

} // namespace Ui::Res::Dock
