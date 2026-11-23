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
 * @file TestUnderlayOf.cpp
 * @brief Unit tests for ui/window/underlayof.h, what stands under a see-through popup: the
 *        window's pixels where the popup is over it, top-down though GL reads them bottom-up, a
 *        content surface's over those, and the popup's background, opaque, where it runs off the
 *        window. Every pixel of the fake window and surface encodes where it stands - red its
 *        column, green its row, blue which of the two - so a shift, a flip or a pixel from the
 *        wrong source fails, which a repeating pattern can hide
 */

#include "ui/color.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"
#include "ui/window/underlayof.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

using Ui::Res::Type::bound_t;
using Ui::Window::underlayOf;

namespace {

using pixel_t = std::array<std::uint8_t, 4>;

constexpr int          WINDOW_W       = 40;
constexpr int          WINDOW_H       = 30;
constexpr std::uint8_t FROM_WINDOW    = 1;                            // blue of the window's pixels
constexpr std::uint8_t FROM_CONTENT   = 2;                            // blue of the content surface's
const Ui::Color        BACKGROUND     = Ui::Color(200, 100, 50, 128); // see-through, as a popup's
const pixel_t          BACKGROUND_PIX = { 200, 100, 50, 255 };        // and opaque in the underlay

// A pixel standing at (x, y) of a source, `source` its blue
pixel_t patternAt(int x, int y, std::uint8_t source)
{
    return { static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), source, 255 };
}

// A region of the window, rows bottom-up as GL reads them
std::vector<std::uint8_t> readWindow(const bound_t & region)
{
    const int                 width  = static_cast<int>(region.w);
    const int                 height = static_cast<int>(region.h);
    std::vector<std::uint8_t> pixels;
    for (int row = height - 1; row >= 0; --row) {
        for (int column = 0; column < width; ++column) {
            const pixel_t pixel = patternAt(static_cast<int>(region.x) + column,
                                            static_cast<int>(region.y) + row,
                                            FROM_WINDOW);
            pixels.insert(pixels.end(), pixel.begin(), pixel.end());
        }
    }
    return pixels;
}

// A region of the content surface, in its own px, rows top-down as its renderer hands them
std::vector<std::uint8_t> readContent(const bound_t & region)
{
    std::vector<std::uint8_t> pixels;
    for (int row = 0; row < static_cast<int>(region.h); ++row) {
        for (int column = 0; column < static_cast<int>(region.w); ++column) {
            const pixel_t pixel = patternAt(static_cast<int>(region.x) + column,
                                            static_cast<int>(region.y) + row,
                                            FROM_CONTENT);
            pixels.insert(pixels.end(), pixel.begin(), pixel.end());
        }
    }
    return pixels;
}

std::vector<std::uint8_t> readNothing(const bound_t & /*region*/) { return {}; }

bool isInWindow(int x, int y) { return x >= 0 && y >= 0 && x < WINDOW_W && y < WINDOW_H; }

// The window's pixel at (x, y) of the main window, the background off it
pixel_t windowOrBackgroundAt(int x, int y) { return isInWindow(x, y) ? patternAt(x, y, FROM_WINDOW) : BACKGROUND_PIX; }

// Every pixel of `underlay` against `expected(x, y)`, x and y in main-window px: "" when all match,
// else the first that does not and how many
template <typename Expected>
std::string mismatchesOf(const std::vector<std::uint8_t> & underlay, const bound_t & area, const Expected & expected)
{
    const int areaW = static_cast<int>(area.w);
    const int areaH = static_cast<int>(area.h);
    if (underlay.size() != static_cast<std::size_t>(areaW) * areaH * 4) {
        return "the underlay is " + std::to_string(underlay.size()) + " bytes";
    }
    int         misses = 0;
    std::string first;
    for (int row = 0; row < areaH; ++row) {
        for (int column = 0; column < areaW; ++column) {
            const std::size_t at   = ((static_cast<std::size_t>(row) * areaW) + column) * 4;
            const pixel_t     got  = { underlay.at(at), underlay.at(at + 1), underlay.at(at + 2), underlay.at(at + 3) };
            const int         x    = static_cast<int>(area.x) + column;
            const int         y    = static_cast<int>(area.y) + row;
            const pixel_t     want = expected(x, y);
            if (got != want) {
                if (misses == 0) {
                    first = "at " + std::to_string(x) + "," + std::to_string(y) + " got " + std::to_string(got.at(0))
                          + "," + std::to_string(got.at(1)) + "," + std::to_string(got.at(2)) + " want "
                          + std::to_string(want.at(0)) + "," + std::to_string(want.at(1)) + ","
                          + std::to_string(want.at(2));
                }
                ++misses;
            }
        }
    }
    return misses == 0 ? std::string {} : first + " (" + std::to_string(misses) + " pixels)";
}

} // namespace

TEST(UnderlayOfTest, InsideTheWindowItIsTheWindow)
{
    const bound_t area { .x = 10.0F, .y = 5.0F, .w = 8.0F, .h = 6.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, std::nullopt, readNothing);
    EXPECT_EQ(mismatchesOf(underlay, area, windowOrBackgroundAt), "");
}

// GL hands its rows bottom-up: the underlay's first row is still the area's top one
TEST(UnderlayOfTest, ItsFirstRowIsTheAreasTop)
{
    const bound_t area { .x = 0.0F, .y = 7.0F, .w = 1.0F, .h = 3.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, std::nullopt, readNothing);
    ASSERT_EQ(underlay.size(), 12U);
    EXPECT_EQ(underlay.at(1), 7); // the top row's green, its row
    EXPECT_EQ(underlay.at(9), 9); // the bottom row's
}

// Left of and above the window, the popup's background - opaque, though the popup's is see-through
TEST(UnderlayOfTest, OffTheTopLeftItIsTheBackground)
{
    const bound_t area { .x = -4.0F, .y = -3.0F, .w = 10.0F, .h = 8.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, std::nullopt, readNothing);
    EXPECT_EQ(mismatchesOf(underlay, area, windowOrBackgroundAt), "");
}

TEST(UnderlayOfTest, OffTheBottomRightItIsTheBackground)
{
    const bound_t area { .x = 35.0F, .y = 25.0F, .w = 10.0F, .h = 10.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, std::nullopt, readNothing);
    EXPECT_EQ(mismatchesOf(underlay, area, windowOrBackgroundAt), "");
}

TEST(UnderlayOfTest, WhollyOffTheWindowItIsAllBackground)
{
    const bound_t area { .x = 50.0F, .y = 40.0F, .w = 5.0F, .h = 5.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, std::nullopt, readNothing);
    EXPECT_EQ(mismatchesOf(underlay, area, windowOrBackgroundAt), "");
}

// A content surface of its own over part of the area: its pixels there, read top-down in its own
// px; the window's round it
TEST(UnderlayOfTest, AContentSurfaceIsOverTheWindow)
{
    const bound_t content { .x = 20.0F, .y = 10.0F, .w = 10.0F, .h = 10.0F };
    const bound_t area { .x = 15.0F, .y = 5.0F, .w = 20.0F, .h = 20.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, content, readContent);
    EXPECT_EQ(mismatchesOf(underlay,
                           area,
                           [&content](int x, int y) {
                               const bool isOverContent = x >= 20 && y >= 10 && x < 30 && y < 20;
                               return isOverContent ? patternAt(x - static_cast<int>(content.x),
                                                                y - static_cast<int>(content.y),
                                                                FROM_CONTENT)
                                                    : windowOrBackgroundAt(x, y);
                           }),
              "");
}

// A content surface clear of the area is never read
TEST(UnderlayOfTest, AContentSurfaceClearOfTheAreaIsNotRead)
{
    const bound_t content { .x = 30.0F, .y = 20.0F, .w = 5.0F, .h = 5.0F };
    const bound_t area { .x = 0.0F, .y = 0.0F, .w = 10.0F, .h = 10.0F };
    bool          isRead = false;
    const auto    underlay =
        underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, content, [&isRead](const bound_t & region) {
            isRead = true;
            return readContent(region);
        });
    EXPECT_FALSE(isRead);
    EXPECT_EQ(mismatchesOf(underlay, area, windowOrBackgroundAt), "");
}

// A surface that hands back the wrong size is left out: the window's pixels stay
TEST(UnderlayOfTest, AContentReadOfTheWrongSizeIsLeftOut)
{
    const bound_t content { .x = 20.0F, .y = 10.0F, .w = 10.0F, .h = 10.0F };
    const bound_t area { .x = 15.0F, .y = 5.0F, .w = 20.0F, .h = 20.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readWindow, content, readNothing);
    EXPECT_EQ(mismatchesOf(underlay, area, windowOrBackgroundAt), "");
}

// A window that hands back the wrong size is left out: the background, opaque, everywhere
TEST(UnderlayOfTest, AWindowReadOfTheWrongSizeIsLeftOut)
{
    const bound_t area { .x = 10.0F, .y = 5.0F, .w = 8.0F, .h = 6.0F };
    const auto    underlay = underlayOf(area, WINDOW_W, WINDOW_H, BACKGROUND, readNothing, std::nullopt, readNothing);
    EXPECT_EQ(mismatchesOf(underlay, area, [](int /*x*/, int /*y*/) { return BACKGROUND_PIX; }), "");
}

TEST(UnderlayOfTest, NoAreaNoPixels)
{
    const auto pixels = underlayOf({ .x = 5.0F, .y = 5.0F, .w = 0.0F, .h = 4.0F },
                                   WINDOW_W,
                                   WINDOW_H,
                                   BACKGROUND,
                                   readWindow,
                                   std::nullopt,
                                   readNothing);
    EXPECT_TRUE(pixels.empty());
}
