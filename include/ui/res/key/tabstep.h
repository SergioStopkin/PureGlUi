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

// CycleTab's "arg" values in shortcut.json - the single source of truth for their
// spelling, resolved through tabStepKeyName(). Named as the keys that take them,
// Ctrl + Prior and Ctrl + Next
enum class TabStepKey : unsigned char {
    Prior, // the tab left of the active one, the last before the first
    Next,  // the tab right of it, the first after the last
};

[[nodiscard]] inline std::string tabStepKeyName(TabStepKey key)
{
    switch (key) {
    case TabStepKey::Prior: return "prior";
    case TabStepKey::Next: return "next";
    }
    return {};
}

} // namespace Ui::Res::Key
