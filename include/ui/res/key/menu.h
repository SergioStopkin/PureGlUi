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

#include <string>

namespace Ui::Res::Key {

// res/menu/*.json + res/button/*.json keys the MenuStore reads - the single
// source of truth for their spelling, resolved through menuKeyName(). These
// are fw-invented config keys, so they are camelCase. Icon roles and action
// keys named INSIDE these files are opaque values (data-driven), not enumerated
// here.
enum class MenuKey : unsigned char {
    // Menu / item / button node fields.
    Label,     // display label (locale key)
    Action,    // actionKey to dispatch
    Shortcut,  // shortcut spec
    Icon,      // icon .svg (or role fallback resolved elsewhere)
    Separator, // item is a separator rule
    Enabled,   // item starts enabled
    Visible,   // item is shown
    Order,     // sort order among siblings
    Items,     // child item array
    Tooltip,   // button tooltip
    Anchor,    // toolbar edge a button sits on ("left" / "right")
    Width,     // button / dialog width
    Height,    // button / dialog height
    Name,      // auto-submenu entry identity (res/submenu/<key>/*.json)
    // "submenus" block: array of children, or {auto,action} auto-generation.
    Submenus, // submenu spec (array or object)
    Auto,     // object form: source subdir key under res/submenu/
    // Inline "dialog" block on a menu item (a different schema from
    // res/dialog.json - that file's keys live in DialogKey).
    Dialog,  // inline dialog block
    Type,    // dialog type name (Info/Warning/...)
    Title,   // dialog title (locale key)
    Content, // dialog body text (locale key)
    Link,    // dialog hyperlink
    File,    // dialog body loaded from a file
};

[[nodiscard]] inline std::string menuKeyName(MenuKey key)
{
    switch (key) {
    case MenuKey::Label: return "label";
    case MenuKey::Action: return "action";
    case MenuKey::Shortcut: return "shortcut";
    case MenuKey::Icon: return "icon";
    case MenuKey::Separator: return "separator";
    case MenuKey::Enabled: return "enabled";
    case MenuKey::Visible: return "visible";
    case MenuKey::Order: return "order";
    case MenuKey::Items: return "items";
    case MenuKey::Tooltip: return "tooltip";
    case MenuKey::Anchor: return "anchor";
    case MenuKey::Width: return "width";
    case MenuKey::Height: return "height";
    case MenuKey::Name: return "name";
    case MenuKey::Submenus: return "submenus";
    case MenuKey::Auto: return "auto";
    case MenuKey::Dialog: return "dialog";
    case MenuKey::Type: return "type";
    case MenuKey::Title: return "title";
    case MenuKey::Content: return "content";
    case MenuKey::Link: return "link";
    case MenuKey::File: return "file";
    }
    return {};
}

} // namespace Ui::Res::Key
