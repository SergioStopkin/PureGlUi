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
#include "ui/res/type/colorpair.h"
#include "ui/type.h"

#include <string>

namespace Ui::Res::Type {

struct alignas(128) button_t final {
    id_t                        id     = INVALID_ID; // numeric runtime id (hit-test, click routing, m_actionMap)
    fpx_t                       width  = 0;
    fpx_t                       height = 0;
    int16_t                     order  = 0;
    Ui::Res::Type::color_pair_t colors;
    std::string                 actionKey;
    std::string                 label;
    std::string                 tooltip;
    bool                        enabled = true;
    bool                        visible = true;
    std::string                 icon;
    // Which toolbar edge this button sits on. DockAnchor rather than a third
    // Left/Right enum - it already means "screen edge" and carries the JSON
    // name mapping, even though it is named for the dock that first needed it
    Ui::Res::Dock::DockAnchor anchor = Ui::Res::Dock::DockAnchor::Left;
    Ui::key_t                 key; // authored hierarchical identity (forward-looking; "" = none)

    bool operator==(const button_t &) const = default;
};

} // namespace Ui::Res::Type
