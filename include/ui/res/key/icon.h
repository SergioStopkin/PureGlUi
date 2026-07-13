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

// icon-defaults.json entry fields - the single source of truth for their
// spelling, resolved through iconKeyName().
enum class IconKey : unsigned char {
    Icon,  // .svg filename (alias) or the name of another entry (role)
    Place, // icon placement relative to the label (default Left)
};

[[nodiscard]] inline std::string iconKeyName(IconKey key)
{
    switch (key) {
    case IconKey::Icon: return "icon";
    case IconKey::Place: return "place";
    }
    return {};
}

} // namespace Ui::Res::Key
