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
 * @file TestScrollBar.cpp
 * @brief Unit tests for Ui::Render::ScrollBar, the bar the dialog and every dock
 *        share. Covers the two geometries (hit area at hover width, drawing at
 *        the current state's), the state-driven radius/min-thumb/colours, and
 *        each gesture: hover, thumb grab, track paging, drag and release.
 *
 * A real ResManager over the shipped res/ supplies the "scrollbar" block, so the
 * tests assert relationships (hit >= draw, hover != idle) rather than the px
 * values in that file, which are the designer's to change.
 */

#include "ui/render/scrollbar.h"
#include "ui/res/resmanager.h"

#include <gtest/gtest.h>
#include <memory>

using Ui::fpx_t;
using Ui::Render::ScrollBar;
using Ui::Res::ResManager;
using Ui::Res::Type::bound_t;

namespace {

// One area for every test: 200 wide, 100 tall, 400 of content in a 100 viewport,
// so exactly 300 is scrollable and the thumb is a quarter of the track.
constexpr fpx_t AREA_W    = 200.0F;
constexpr fpx_t AREA_H    = 100.0F;
constexpr fpx_t CONTENT_H = 400.0F;
constexpr fpx_t MAX_VALUE = CONTENT_H - AREA_H;

class ScrollBarTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        s_resManager = std::make_unique<ResManager>(TEST_RES_DIR);
        s_resManager->loadAll();
    }

    static void TearDownTestSuite() { s_resManager.reset(); }

    // Scrollable by default; pass a viewport >= content for the inert case
    static ScrollBar make(fpx_t contentHeight = CONTENT_H)
    {
        ScrollBar bar(*s_resManager);
        bar.setGeometry({ 0, 0, AREA_W, AREA_H }, contentHeight, AREA_H);
        return bar;
    }

    static std::unique_ptr<ResManager> s_resManager;
};

std::unique_ptr<ResManager> ScrollBarTest::s_resManager;

} // namespace

// ============================================================================
// Geometry
// ============================================================================

TEST_F(ScrollBarTest, TrackSitsAtTheRightEdgeOfTheArea)
{
    const ScrollBar bar   = make();
    const bound_t   track = bar.track();
    EXPECT_GT(track.x, AREA_W / 2.0F);
    EXPECT_LE(track.x + track.w, AREA_W + std::abs(s_resManager->layout().scrollbarRight));
    EXPECT_FLOAT_EQ(track.y, 0.0F);
    EXPECT_FLOAT_EQ(track.h, AREA_H);
}

TEST_F(ScrollBarTest, HitAreaIsAtLeastAsWideAsTheDrawnBar)
{
    const ScrollBar bar = make();
    // Idle: the drawn bar is the narrow one, the hit area stays at hover width,
    // which is what keeps the pointer inside a bar that widens under it
    EXPECT_GE(bar.metrics().track.w, bar.track().w);
}

TEST_F(ScrollBarTest, ThumbIsProportionalAndTravelsWithTheValue)
{
    const ScrollBar bar = make();
    const bound_t   top = bar.thumb(0.0F);
    EXPECT_FLOAT_EQ(top.y, bar.track().y);
    EXPECT_LT(top.h, bar.track().h);

    const bound_t bottom = bar.thumb(MAX_VALUE);
    EXPECT_FLOAT_EQ(bottom.y + bottom.h, bar.track().y + bar.track().h);
    EXPECT_FLOAT_EQ(bottom.h, top.h);
}

TEST_F(ScrollBarTest, ThumbColumnFollowsTheDrawnBarNotTheHitArea)
{
    const ScrollBar bar = make();
    EXPECT_FLOAT_EQ(bar.thumb(0.0F).x, bar.track().x);
    EXPECT_FLOAT_EQ(bar.thumb(0.0F).w, bar.track().w);
}

// Negative: nothing to scroll means the thumb fills the track, so no gesture can
// move it and the caller can still draw the bar without special-casing
TEST_F(ScrollBarTest, ContentThatFitsGivesAFullLengthThumb)
{
    const ScrollBar bar = make(AREA_H);
    EXPECT_FLOAT_EQ(bar.thumb(0.0F).h, bar.track().h);
}

// Negative: a zero-height area must not produce a NaN or an inverted rect
TEST_F(ScrollBarTest, EmptyAreaKeepsGeometryFinite)
{
    ScrollBar bar(*s_resManager);
    bar.setGeometry({ 0, 0, 0, 0 }, 0.0F, 0.0F);
    EXPECT_FLOAT_EQ(bar.track().h, 0.0F);
    EXPECT_FLOAT_EQ(bar.thumb(0.0F).h, 0.0F);
}

// ============================================================================
// Hover state
// ============================================================================

TEST_F(ScrollBarTest, HoverOverTheTrackChangesStateOnce)
{
    ScrollBar     bar     = make();
    const bound_t track   = bar.metrics().track;
    const fpx_t   insideX = track.x + (track.w / 2.0F);

    EXPECT_FALSE(bar.isHovered());
    EXPECT_TRUE(bar.onMouseMove(insideX, track.y + 1.0F, 0.0F));
    EXPECT_TRUE(bar.isHovered());
    // Same position again: nothing moved, so nothing to repaint
    EXPECT_FALSE(bar.onMouseMove(insideX, track.y + 1.0F, 0.0F));
}

TEST_F(ScrollBarTest, HoverWidensTheBarAndSwapsRadius)
{
    ScrollBar     bar   = make();
    const fpx_t   idleW = bar.track().w;
    const auto    idleR = bar.radius();
    const bound_t track = bar.metrics().track;

    bar.onMouseMove(track.x + (track.w / 2.0F), track.y + 1.0F, 0.0F);
    bar.setGeometry({ 0, 0, AREA_W, AREA_H }, CONTENT_H, AREA_H); // restated per frame
    EXPECT_GE(bar.track().w, idleW);
    EXPECT_NE(bar.radius() == idleR, bar.track().w != idleW);
}

TEST_F(ScrollBarTest, PointerOffTheTrackClearsHover)
{
    ScrollBar     bar   = make();
    const bound_t track = bar.metrics().track;
    bar.onMouseMove(track.x + 1.0F, track.y + 1.0F, 0.0F);
    ASSERT_TRUE(bar.isHovered());

    EXPECT_TRUE(bar.onMouseMove(0.0F, track.y + 1.0F, 0.0F));
    EXPECT_FALSE(bar.isHovered());
}

TEST_F(ScrollBarTest, ThumbColourFollowsTheThumbNotTheTrack)
{
    ScrollBar     bar   = make();
    const bound_t track = bar.metrics().track;
    const bound_t thumb = bar.thumb(0.0F);

    const Ui::Color idle = bar.thumbColor();
    // Over the track but well below the thumb: the bar is hovered, the thumb is not
    bar.onMouseMove(track.x + 1.0F, track.y + track.h - 1.0F, 0.0F);
    EXPECT_TRUE(bar.isHovered());
    EXPECT_EQ(bar.thumbColor(), idle);

    bar.onMouseMove(track.x + 1.0F, thumb.y + (thumb.h / 2.0F), 0.0F);
    EXPECT_EQ(bar.thumbColor(), s_resManager->theme().scrollbarThumbHover);
}

TEST_F(ScrollBarTest, ClearDropsEveryState)
{
    ScrollBar     bar   = make();
    const bound_t track = bar.metrics().track;
    bar.onMouseMove(track.x + 1.0F, track.y + 1.0F, 0.0F);
    bar.press(track.y + 1.0F, 0.0F, 0.0F);
    ASSERT_TRUE(bar.isDragging());

    bar.clear();
    EXPECT_FALSE(bar.isDragging());
    EXPECT_FALSE(bar.isHovered());
}

// ============================================================================
// Gestures
// ============================================================================

TEST_F(ScrollBarTest, PressOnTheThumbGrabsAndKeepsTheValue)
{
    ScrollBar     bar   = make();
    const bound_t thumb = bar.thumb(0.0F);

    const fpx_t value = bar.press(thumb.y + (thumb.h / 2.0F), 0.0F, 0.0F);
    EXPECT_TRUE(bar.isDragging());
    EXPECT_FLOAT_EQ(value, 0.0F);
}

TEST_F(ScrollBarTest, PressBelowTheThumbPagesForwardWithoutGrabbing)
{
    ScrollBar     bar   = make();
    const bound_t track = bar.metrics().track;

    const fpx_t value = bar.press(track.y + track.h - 1.0F, 0.0F, 0.0F);
    EXPECT_FALSE(bar.isDragging());
    EXPECT_FLOAT_EQ(value, AREA_H); // exactly one viewport
}

TEST_F(ScrollBarTest, PressAboveTheThumbPagesBack)
{
    ScrollBar     bar   = make();
    const bound_t thumb = bar.thumb(MAX_VALUE);

    const fpx_t value = bar.press(thumb.y - 1.0F, MAX_VALUE, MAX_VALUE);
    EXPECT_FALSE(bar.isDragging());
    EXPECT_FLOAT_EQ(value, MAX_VALUE - AREA_H);
}

// Paging steps from `pageFrom`, which for an animated caller is where it is
// heading rather than the frame still catching up
TEST_F(ScrollBarTest, PagingUsesPageFromNotTheVisibleValue)
{
    ScrollBar     bar   = make();
    const bound_t track = bar.metrics().track;

    const fpx_t value = bar.press(track.y + track.h - 1.0F, 0.0F, AREA_H);
    EXPECT_FLOAT_EQ(value, AREA_H * 2.0F);
}

TEST_F(ScrollBarTest, DragMapsPointerTravelOntoContentTravel)
{
    ScrollBar     bar   = make();
    const bound_t thumb = bar.thumb(0.0F);
    const fpx_t   grabY = thumb.y + (thumb.h / 2.0F);
    bar.press(grabY, 0.0F, 0.0F);
    ASSERT_TRUE(bar.isDragging());

    // Thumb travel is shorter than content travel, so a pixel of pointer is
    // worth more than a pixel of scroll
    const fpx_t moved = bar.valueAt(grabY + 10.0F);
    EXPECT_GT(moved, 10.0F);
    EXPECT_LE(moved, MAX_VALUE);
}

TEST_F(ScrollBarTest, DragIsClampedToBothEnds)
{
    ScrollBar     bar   = make();
    const bound_t thumb = bar.thumb(0.0F);
    const fpx_t   grabY = thumb.y + (thumb.h / 2.0F);
    bar.press(grabY, 0.0F, 0.0F);

    EXPECT_FLOAT_EQ(bar.valueAt(grabY - 1000.0F), 0.0F);
    EXPECT_FLOAT_EQ(bar.valueAt(grabY + 1000.0F), MAX_VALUE);
}

TEST_F(ScrollBarTest, ReleaseEndsTheDragButKeepsHover)
{
    ScrollBar     bar   = make();
    const bound_t thumb = bar.thumb(0.0F);
    bar.onMouseMove(bar.metrics().track.x + 1.0F, thumb.y + 1.0F, 0.0F);
    bar.press(thumb.y + 1.0F, 0.0F, 0.0F);
    ASSERT_TRUE(bar.isDragging());

    bar.release();
    EXPECT_FALSE(bar.isDragging());
    EXPECT_TRUE(bar.isHovered());
}

// Negative: a press outside the bar is not the bar's business - contains() is
// what the caller gates on, and it must say no
TEST_F(ScrollBarTest, ContainsRejectsPositionsOutsideTheHitArea)
{
    const ScrollBar bar   = make();
    const bound_t   track = bar.metrics().track;
    EXPECT_TRUE(bar.contains(track.x + 1.0F, track.y + 1.0F));
    EXPECT_FALSE(bar.contains(track.x - 5.0F, track.y + 1.0F));
    EXPECT_FALSE(bar.contains(track.x + 1.0F, track.y + track.h + 5.0F));
}
