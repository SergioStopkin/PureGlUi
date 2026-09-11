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

#include "ui/res/dock/rowkind.h"
#include "ui/type.h"

#include <string>

namespace Ui::Res::Dock {

// One line of dock content. The host projects its domain into a list of these
// the way it projects workspaces into Ui::TabBar - the framework renders and
// hit-tests them and never learns what they mean.
//
// Deliberately one type for both dock kinds rather than a tree row and a
// property row:
//   tree     - depth > 0 indents, hasChildren draws the chevron, value empty
//   property - depth 0, no chevron, value non-empty and drawn right-aligned
//   group    - a property row with hasChildren, so groups collapse for free
//
// `id` is the host's own identifier, echoed back untouched in the row intents.
// The framework treats it as opaque, so a host can put a part id, a feature
// index or a property key in it.
// `label` and `value` are LITERAL display text, not locale keys - a row is a
// projection carrying host content (a part name), exactly as tab_t::label does.
// The host resolves any locale lookup before projecting. Menu items are the
// other case: those are res-loaded definitions whose labels ARE locale keys.
//
// `id` is INVALID_ID for a row that should not be interactive - a key/value
// line with nothing to select. Such a row still renders, but never reports.
struct alignas(128) row_t final {
    id_t        id = INVALID_ID;
    std::string label;
    std::string value; // right-aligned second column; empty for a tree row
    std::string icon;  // optional leading svg; empty for none
    uint16_t    depth       = 0;
    bool        hasChildren = false;
    bool        isExpanded  = false;
    bool        isSelected  = false;
    RowKind     kind        = RowKind::Text;
    // Slider position, 0..1. The framework only knows the fraction; the host
    // maps it to whatever range it means and can put the mapped number in
    // `value` to have it drawn alongside. Ignored unless kind is Slider.
    fpx_t ratio = 0.0F;

    bool operator==(const row_t &) const = default;
};

} // namespace Ui::Res::Dock
