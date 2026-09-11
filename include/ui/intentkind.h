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

#include <cstdint>

namespace Ui {

// What the framework asks the host to do in response to input. The fw only
// emits these; the host executes them (dispatch a command, open a window,
// mutate workspaces, touch the OS). Anything the fw can do itself - layout,
// hover, tab scroll, menu highlight - is not an intent (it sets result.dirty).
enum class IntentKind : uint8_t {
    EmitAction,  // run a host command: actionKey (+ arg)
    OpenPopup,   // open the top-menu dropdown for a menu id
    ClosePopup,  // dismiss the open popup/submenu
    OpenSubmenu, // open/switch the submenu for an item id (hover-driven)
    OpenDialog,  // open the dialog attached to an item id
    SwitchTab,   // activate workspace/tab id
    CloseTab,    // close workspace/tab id
    CopyText,    // copy arg to the clipboard (status-bar text)
    // Dock rows are host-projected, so both of these are host work: the row
    // tree and its expanded flags live in the host's model, and it re-projects
    // via setDockRows in response. `id` is the host's own row id, opaque here -
    // which is why it must be unique across docks.
    ActivateRow, // row id selected
    ToggleRow,   // row id expand/collapse requested
};

} // namespace Ui
