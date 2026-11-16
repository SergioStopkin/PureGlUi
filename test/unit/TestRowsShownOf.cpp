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

/**
 * @file TestRowsShownOf.cpp
 * @brief Unit tests for ui/render/rowsshownof.h, how many rows the Ctrl+Tab list shows: the
 *        count its window was made for, at every scale - its height put on the physical grid
 *        and read back, as the list's window and renderer take it - never a row short.
 */

#include "ui/config.h"
#include "ui/render/rowsshownof.h"
#include "ui/type.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>

using Ui::Render::rowsShownOf;

namespace {

constexpr Ui::fpx_t ROW       = 32.0F; // the shipped tab-switcher-item height
constexpr int       MOST_ROWS = 20;
constexpr int       DPI_FROM  = 96;  // 1x
constexpr int       DPI_TO    = 288; // 3x

} // namespace

TEST(RowsShownOfTest, NoRowHeightShowsNone) { EXPECT_EQ(rowsShownOf(100.0F, 0.0F), 0U); }

TEST(RowsShownOfTest, AWholeNumberOfRowsShowsThem) { EXPECT_EQ(rowsShownOf(ROW * 3.0F, ROW), 3U); }

// openTabSwitcher makes the window rows * ROW tall on the physical grid (toPhysRound) and the
// renderer reads it back (toCss): at every DPI the count comes back as it went in
TEST(RowsShownOfTest, EveryScaleShowsTheRowsTheWindowWasMadeFor)
{
    int         misses = 0;
    std::string first;
    for (int dpi = DPI_FROM; dpi <= DPI_TO; ++dpi) {
        const Ui::config_t config { .dpi = dpi, .scale = dpi / 96.0F };
        for (int rows = 1; rows <= MOST_ROWS; ++rows) {
            const Ui::fpx_t height = config.toCss(config.toPhysRound(rows * ROW));
            if (rowsShownOf(height, ROW) != static_cast<std::size_t>(rows)) {
                if (misses == 0) {
                    first = "dpi " + std::to_string(dpi) + ", " + std::to_string(rows) + " rows";
                }
                ++misses;
            }
        }
    }
    EXPECT_EQ(misses, 0) << "a row short, first at " << first;
}
