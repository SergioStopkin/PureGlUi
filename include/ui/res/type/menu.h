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

#include "ui/res/type/dialog.h"
#include "ui/res/type/iconplace.h"
#include "ui/type.h"

#include <string>
#include <vector>

namespace Ui::Res::Type {

// One node in the menu tree. The same type at every depth: a top-level bar
// entry, a dropdown row, a separator, or a submenu - distinguished only by
// which fields are set and whether `items` is empty. Recursive via `items`.
struct alignas(128) menu_t final {
    id_t                     id    = INVALID_ID; // numeric runtime id (hit-test, click routing, m_actionMap)
    int16_t                  order = 0;          // bar ordering; top-level only, 0 when nested
    std::string              actionKey; // canonical action key ("" = none); dispatched via Ui::Action::Registry
    bool                     showsThemePreview = false; // render hint: draw theme-preview swatch
    std::string              label; // value + display: get(label) -> display text, and the value passed to the action
    bool                     visible     = true;
    bool                     enabled     = true;
    bool                     separator   = false; // render as a separator line
    fpx_t                    popupHeight = 0;     // precomputed dropdown content height (CSS px); top-level only
    std::string              submenu;             // auto-key: subdir under res/submenu/ to auto-populate children
    std::string              submenuActionKey;    // canonical action key attached to each auto-generated child
    std::string              shortcut;            // display text (e.g. "Ctrl+O")
    std::string              icon;                // SVG icon filename
    Ui::Res::Type::IconPlace iconPlace = Ui::Res::Type::IconPlace::Left; // icon position relative to the label
    Ui::Res::Type::dialog_t  dialog;                                     // dialog config (empty title = no dialog)
    std::vector<menu_t>      items;                                      // child nodes (empty = leaf)
    Ui::key_t key; // authored hierarchical identity, e.g. "view:displayMode:shaded" (forward-looking; "" = none)

    bool operator==(const menu_t &) const = default;
};

} // namespace Ui::Res::Type
