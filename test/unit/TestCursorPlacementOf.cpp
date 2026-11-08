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
 * @file TestCursorPlacementOf.cpp
 * @brief Unit tests for ui/window/cursorplacementof.h: a Wayland cursor loaded at a scale stands
 *        on its surface at that scale, its hotspot in surface px, and an image that cannot stand
 *        at it at the scale that stands it nearest the cursor's size
 */

#include "ui/window/cursorplacementof.h"

#include <gtest/gtest.h>

using Ui::Window::cursor_placement_t;
using Ui::Window::cursorPlacementOf;

// The resize cursor at scale 2, 48 px with its hotspot in the middle. Shown at scale 1 that
// middle was 24 surface px in, and the compositor drew the image half its width left of the
// pointer and half its height above it
TEST(CursorPlacementOf, AnImageLoadedAtAScaleStandsAtThatScale)
{
    const cursor_placement_t placement = cursorPlacementOf(48, 48, 24, 24, 2);
    EXPECT_EQ(placement.bufferScale, 2);
    EXPECT_EQ(placement.hotspotX, 12);
    EXPECT_EQ(placement.hotspotY, 12);
}

TEST(CursorPlacementOf, AtScale1AnImageStandsAsLoaded)
{
    const cursor_placement_t placement = cursorPlacementOf(24, 24, 12, 12, 1);
    EXPECT_EQ(placement.bufferScale, 1);
    EXPECT_EQ(placement.hotspotX, 12);
    EXPECT_EQ(placement.hotspotY, 12);
}

// Negative: a theme without the size asked for hands the nearest, and a buffer that does not
// divide by its scale is a protocol error that ends the app
TEST(CursorPlacementOf, AnImageThatDoesNotDivideByTheScaleStaysAt1)
{
    const cursor_placement_t placement = cursorPlacementOf(32, 32, 16, 16, 3);
    EXPECT_EQ(placement.bufferScale, 1);
    EXPECT_EQ(placement.hotspotX, 16);
    EXPECT_EQ(placement.hotspotY, 16);

    EXPECT_EQ(cursorPlacementOf(48, 45, 24, 22, 2).bufferScale, 1);
}

// Negative: a theme without 48 px hands its 24 px image at scale 2. It divides by 2, but at
// that scale it would stand half the size the desktop's cursor does
TEST(CursorPlacementOf, AnImageSmallerThanAskedForStaysAt1)
{
    const cursor_placement_t placement = cursorPlacementOf(24, 24, 12, 12, 2);
    EXPECT_EQ(placement.bufferScale, 1);
    EXPECT_EQ(placement.hotspotX, 12);
    EXPECT_EQ(placement.hotspotY, 12);
}

// At scale 3 a theme without 72 px hands its 64 px image. At 1 it stood 64 surface px, near
// three times the desktop's cursor; at 2, which it divides by, it stands 32
TEST(CursorPlacementOf, AnImageSmallerThanAskedForStandsAtTheScaleNearestItsSize)
{
    const cursor_placement_t placement = cursorPlacementOf(64, 64, 32, 32, 3);
    EXPECT_EQ(placement.bufferScale, 2);
    EXPECT_EQ(placement.hotspotX, 16);
    EXPECT_EQ(placement.hotspotY, 16);
}

// At scale 4 the same image stands 32 at 2 and 16 at 4, as far from 24 each: the smaller scale,
// with the larger image, wins
TEST(CursorPlacementOf, OfTwoScalesAsNearTheSmallerOneWins)
{
    EXPECT_EQ(cursorPlacementOf(64, 64, 32, 32, 4).bufferScale, 2);
}

// Negative: a scale below 1 truncates to 0, which no image divides by
TEST(CursorPlacementOf, AScaleBelow1StaysAt1)
{
    const cursor_placement_t placement = cursorPlacementOf(24, 24, 12, 12, 0);
    EXPECT_EQ(placement.bufferScale, 1);
    EXPECT_EQ(placement.hotspotX, 12);
    EXPECT_EQ(placement.hotspotY, 12);
}
