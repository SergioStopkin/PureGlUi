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
 * @file TestOutermostFirstOf.cpp
 * @brief Ui::Window::outermostFirstOf - the configured docks outermost first, the order
 *        WindowManager holds them in and packs them from the window's edges: the
 *        highest order first, ties as configured, each edge's own order kept among the
 *        other's. Pure, no window.
 */

#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/window/outermostfirstof.h"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

using Ui::Res::Dock::dock_config_t;
using Ui::Res::Dock::DockAnchor;
using Ui::Window::outermostFirstOf;

namespace {

// A dock on `anchor` at `order`
dock_config_t dockAt(DockAnchor anchor, int order)
{
    return { .name = {}, .anchor = anchor, .order = order, .defaultWidth = 100.0F };
}

// The orders of `configs` taken in `indices`, to read a result by
std::vector<int> ordersIn(const std::vector<dock_config_t> & configs, const std::vector<std::size_t> & indices)
{
    std::vector<int> orders;
    for (const std::size_t index : indices) {
        orders.emplace_back(configs.at(index).order);
    }
    return orders;
}

} // namespace

// The highest order first: the outermost packs against its toolbar
TEST(OutermostFirstOf, TheHighestOrderComesFirst)
{
    EXPECT_EQ(
        outermostFirstOf({ dockAt(DockAnchor::Left, 1), dockAt(DockAnchor::Left, 3), dockAt(DockAnchor::Left, 2) }),
        (std::vector<std::size_t> { 1, 2, 0 }));
}

// Ties stay as configured, on one edge or across both
TEST(OutermostFirstOf, TiesStayAsConfigured)
{
    EXPECT_EQ(
        outermostFirstOf({ dockAt(DockAnchor::Left, 1), dockAt(DockAnchor::Right, 1), dockAt(DockAnchor::Left, 1) }),
        (std::vector<std::size_t> { 0, 1, 2 }));
    EXPECT_EQ(outermostFirstOf({ dockAt(DockAnchor::Left, 2),
                                 dockAt(DockAnchor::Left, 1),
                                 dockAt(DockAnchor::Right, 2),
                                 dockAt(DockAnchor::Right, 1) }),
              (std::vector<std::size_t> { 0, 2, 1, 3 }));
}

// Either edge alike: each edge's docks come outermost first among the other edge's
TEST(OutermostFirstOf, EachEdgeKeepsItsOwnOrder)
{
    const std::vector<dock_config_t> configs   = { dockAt(DockAnchor::Left, 1),
                                                   dockAt(DockAnchor::Right, 2),
                                                   dockAt(DockAnchor::Left, 3),
                                                   dockAt(DockAnchor::Right, 1),
                                                   dockAt(DockAnchor::Left, 2) };
    const std::vector<std::size_t>   outermost = outermostFirstOf(configs);
    std::vector<int>                 left;
    std::vector<int>                 right;
    for (const std::size_t index : outermost) {
        (configs.at(index).anchor == DockAnchor::Left ? left : right).emplace_back(configs.at(index).order);
    }
    EXPECT_EQ(left, (std::vector<int> { 3, 2, 1 }));
    EXPECT_EQ(right, (std::vector<int> { 2, 1 }));
}

// Every dock once, however many and in whatever order configured
TEST(OutermostFirstOf, EveryDockOnce)
{
    std::vector<dock_config_t> configs;
    for (const int order : { 4, 1, 7, 1, 3, 9, 4, 2, 8, 5 }) {
        configs.emplace_back(dockAt(order % 2 == 0 ? DockAnchor::Left : DockAnchor::Right, order));
    }
    std::vector<std::size_t> outermost = outermostFirstOf(configs);
    EXPECT_TRUE(std::ranges::is_sorted(ordersIn(configs, outermost), std::ranges::greater {}));
    std::ranges::sort(outermost);
    EXPECT_EQ(outermost, (std::vector<std::size_t> { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }));
}

// Negative: no docks configured, none
TEST(OutermostFirstOf, NoDocksAreNone) { EXPECT_TRUE(outermostFirstOf({}).empty()); }

// Negative: one dock is itself, whatever its order
TEST(OutermostFirstOf, OneDockIsItself)
{
    EXPECT_EQ(outermostFirstOf({ dockAt(DockAnchor::Right, 5) }), (std::vector<std::size_t> { 0 }));
}

// Negative: an order off the usual 1.. - zero, negative - still ranks, by its value
TEST(OutermostFirstOf, AnyOrderRanksByItsValue)
{
    EXPECT_EQ(
        outermostFirstOf({ dockAt(DockAnchor::Left, 0), dockAt(DockAnchor::Left, -1), dockAt(DockAnchor::Left, 5) }),
        (std::vector<std::size_t> { 2, 0, 1 }));
}
