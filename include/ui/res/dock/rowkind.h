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

namespace Ui::Res::Dock {

// How a dock row presents itself. Text covers every row that is read rather
// than manipulated - tree nodes, key/value properties, group headers - and is
// the default, so a host that never sliders anything ignores this entirely.
//
// An explicit kind rather than inferring "slider" from a sentinel value: a
// slider legitimately sits at any ratio including zero, so no value is free to
// mean "not a slider".
enum class RowKind : unsigned char {
    Text,
    Slider,
};

} // namespace Ui::Res::Dock
