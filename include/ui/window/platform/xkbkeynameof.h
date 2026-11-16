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

#include <array>
#include <string>
#include <xkbcommon/xkbcommon.h>

namespace Ui::Window::Platform {

// The name a key goes by, pressed or released, as X11 names it: from its level without Shift,
// so Shift+Tab is Tab, never ISO_Left_Tab, and a shortcut spelled once matches on every backend;
// a release named too, so a Ctrl going up reads as one. Empty for a key with no symbol
[[nodiscard]] inline std::string xkbKeyNameOf(xkb_state * state, xkb_keymap * keymap, xkb_keycode_t keycode)
{
    const xkb_keysym_t *     levelSyms = nullptr;
    const xkb_layout_index_t layout    = xkb_state_key_get_layout(state, keycode);
    if (xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, 0, &levelSyms) <= 0) {
        return {};
    }
    std::array<char, 64> name {};
    xkb_keysym_get_name(*levelSyms, name.data(), name.size());
    return name.data();
}

} // namespace Ui::Window::Platform
