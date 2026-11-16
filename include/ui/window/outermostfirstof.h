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

#include "ui/res/dock/config.h"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <vector>

namespace Ui::Window {

// The docks `configs` declares, as indices into it, outermost first: the highest order
// first, the order the docks pack in from the window's edges, ties as configured. Either
// edge alike - each packs its own, so one walk places both.
//
// The tie broken by the index, so a plain sort keeps it: a stable sort takes a temporary
// buffer through an API libstdc++ deprecates
[[nodiscard]] inline std::vector<std::size_t>
outermostFirstOf(const std::vector<Ui::Res::Dock::dock_config_t> & configs)
{
    const auto               every = std::views::iota(std::size_t { 0 }, configs.size());
    std::vector<std::size_t> indices(every.begin(), every.end());
    std::ranges::sort(indices, [&configs](std::size_t lhs, std::size_t rhs) {
        const int lhsOrder = configs.at(lhs).order;
        const int rhsOrder = configs.at(rhs).order;
        return lhsOrder != rhsOrder ? lhsOrder > rhsOrder : lhs < rhs;
    });
    return indices;
}

} // namespace Ui::Window
