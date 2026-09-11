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

namespace Ui::Render {

// Which way a one-dimensional widget runs. A thumb on a track is the same
// geometry either way - only which half of a bound_t is "along" it changes - so
// the axis is a parameter rather than two copies of the maths.
enum class Axis : unsigned char {
    Vertical,  // a scrollbar: offset is y, length is h
    Horizontal // a slider: offset is x, length is w
};

} // namespace Ui::Render
