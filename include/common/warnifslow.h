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

#include <iostream>
#include <string_view>

namespace Common {

// A time over `mostMs` warns and never fails: a CI runner sharing its cores oversleeps now
// and then, and one drawing on the CPU is over at every frame. Printed as GitHub Actions'
// own warning, an annotation on the run
inline void warnIfSlow(std::string_view what, double ms, double mostMs)
{
    if (ms >= mostMs) {
        std::cout << "::warning::" << what << " took " << ms << " ms, over " << mostMs << std::endl;
    }
}

} // namespace Common
