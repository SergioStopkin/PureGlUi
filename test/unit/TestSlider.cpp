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
 * @file TestSlider.cpp
 * @brief Unit tests for the dock slider's share of the thumb geometry: the
 *        horizontal axis, a fixed-length thumb over a 0..1 range, and the two
 *        presses (grab the thumb, or jump it to the pointer).
 *
 * The regression these pin down: the drawing kept the thumb inside the track ends
 * while the pointer mapping spanned the whole width, so the thumb trailed the
 * cursor by its own length. Every test below reads the position through the same
 * thumbBound() the renderer calls, so the two cannot drift apart again.
 */

#include "ui/render/axis.h"
#include "ui/render/slider.h"
#include "ui/render/thumbtrack.h"

#include <gtest/gtest.h>

using Ui::fpx_t;
using Ui::Render::Axis;
using Ui::Render::sliderMetrics;
using Ui::Render::thumb_metrics_t;
using Ui::Render::thumb_track_t;
using Ui::Res::Type::bound_t;

namespace {

// One track for every test, offset from the origin on both axes so a mapping that
// forgets to subtract the track's own start shows up
constexpr bound_t TRACK { 100.0F, 50.0F, 200.0F, 10.0F };
// res states the thumb's length in production, so the test owns it too rather
// than deriving it from the track - that coupling is exactly what moved out
constexpr fpx_t THUMB  = 20.0F;
constexpr fpx_t TRAVEL = TRACK.w - THUMB;

// Where the thumb's middle sits for a value, which is what a pointer aims at
[[nodiscard]] fpx_t thumbCentre(const thumb_metrics_t & metrics, fpx_t value)
{
    const bound_t span = thumb_track_t::thumbBound(metrics, value);
    return span.x + (span.w / 2.0F);
}

} // namespace

// ============================================================================
// Geometry
// ============================================================================

TEST(SliderTest, MetricsRunAlongTheTrackWithAFixedThumb)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    EXPECT_EQ(metrics.axis, Axis::Horizontal);
    EXPECT_FLOAT_EQ(metrics.thumbLength, THUMB); // carried through, not derived from the track
    EXPECT_FLOAT_EQ(metrics.maxValue, 1.0F);     // a plain ratio, unlike a scrollbar's pixel range
    EXPECT_FLOAT_EQ(metrics.trackStart(), TRACK.x);
    EXPECT_FLOAT_EQ(metrics.trackLength(), TRACK.w);
    EXPECT_FLOAT_EQ(metrics.travel(), TRAVEL);
}

TEST(SliderTest, ThumbStaysInsideBothTrackEnds)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    const bound_t         start   = thumb_track_t::thumbBound(metrics, 0.0F);
    const bound_t         end     = thumb_track_t::thumbBound(metrics, 1.0F);
    EXPECT_FLOAT_EQ(start.x, TRACK.x);
    EXPECT_FLOAT_EQ(end.x + end.w, TRACK.x + TRACK.w);
}

TEST(SliderTest, ThumbFillsTheTrackAcrossItsOtherAxis)
{
    const bound_t span = thumb_track_t::thumbBound(sliderMetrics(TRACK, THUMB), 0.5F);
    EXPECT_FLOAT_EQ(span.y, TRACK.y);
    EXPECT_FLOAT_EQ(span.h, TRACK.h);
    EXPECT_FLOAT_EQ(span.w, THUMB);
}

TEST(SliderTest, ThumbTravelsProportionallyWithTheValue)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    EXPECT_FLOAT_EQ(thumb_track_t::thumbBound(metrics, 0.5F).x, TRACK.x + (TRAVEL / 2.0F));
    EXPECT_FLOAT_EQ(thumb_track_t::thumbBound(metrics, 0.25F).x, TRACK.x + (TRAVEL / 4.0F));
}

TEST(SliderTest, OutOfRangeValuesClampToTheEnds)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    EXPECT_FLOAT_EQ(thumb_track_t::thumbBound(metrics, -1.0F).x, TRACK.x);
    EXPECT_FLOAT_EQ(thumb_track_t::thumbBound(metrics, 2.0F).x, TRACK.x + TRAVEL);
}

// ============================================================================
// Hit test
// ============================================================================

TEST(SliderTest, IsOnThumbCoversExactlyTheDrawnSpan)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    const bound_t         span    = thumb_track_t::thumbBound(metrics, 0.5F);
    EXPECT_TRUE(thumb_track_t::isOnThumb(metrics, 0.5F, span.x));
    EXPECT_TRUE(thumb_track_t::isOnThumb(metrics, 0.5F, span.x + span.w - 0.1F));
    EXPECT_FALSE(thumb_track_t::isOnThumb(metrics, 0.5F, span.x - 0.1F));
    EXPECT_FALSE(thumb_track_t::isOnThumb(metrics, 0.5F, span.x + span.w));
}

// ============================================================================
// Gestures
// ============================================================================

TEST(SliderTest, PressOnBareTrackCentresTheThumbOnThePointer)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    const fpx_t           pointer = TRACK.x + 140.0F;
    const fpx_t           jumped  = thumb_track_t::centredValue(metrics, pointer);
    EXPECT_FLOAT_EQ(thumbCentre(metrics, jumped), pointer);
    EXPECT_TRUE(thumb_track_t::isOnThumb(metrics, jumped, pointer));
}

TEST(SliderTest, CentredValueClampsWhereTheThumbWouldOverhang)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    EXPECT_FLOAT_EQ(thumb_track_t::centredValue(metrics, TRACK.x), 0.0F);
    EXPECT_FLOAT_EQ(thumb_track_t::centredValue(metrics, TRACK.x + TRACK.w), 1.0F);
}

TEST(SliderTest, DraggingAGrabbedThumbKeepsItUnderThePointer)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    const fpx_t           grabbed = thumbCentre(metrics, 0.25F);

    thumb_track_t drag;
    drag.begin(metrics, grabbed, 0.25F);
    EXPECT_TRUE(drag.isDragging);

    const fpx_t moved = grabbed + 40.0F;
    EXPECT_FLOAT_EQ(thumbCentre(metrics, drag.valueAt(metrics, moved)), moved);
}

TEST(SliderTest, GrabbingOffCentreKeepsThatOffset)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    const bound_t         span    = thumb_track_t::thumbBound(metrics, 0.4F);
    const fpx_t           grabbed = span.x + 1.0F; // near the leading edge, not the middle

    thumb_track_t drag;
    drag.begin(metrics, grabbed, 0.4F);

    const fpx_t moved = grabbed + 25.0F;
    EXPECT_FLOAT_EQ(thumb_track_t::thumbBound(metrics, drag.valueAt(metrics, moved)).x, moved - 1.0F);
}

TEST(SliderTest, DraggingPastAnEndStopsAtIt)
{
    const thumb_metrics_t metrics = sliderMetrics(TRACK, THUMB);
    const fpx_t           grabbed = thumbCentre(metrics, 0.5F);

    thumb_track_t drag;
    drag.begin(metrics, grabbed, 0.5F);
    EXPECT_FLOAT_EQ(drag.valueAt(metrics, grabbed + (TRACK.w * 2.0F)), 1.0F);
    EXPECT_FLOAT_EQ(drag.valueAt(metrics, grabbed - (TRACK.w * 2.0F)), 0.0F);
}

TEST(SliderTest, ClearEndsTheDrag)
{
    thumb_track_t drag;
    drag.begin(sliderMetrics(TRACK, THUMB), TRACK.x, 0.0F);
    drag.clear();
    EXPECT_FALSE(drag.isDragging);
}

// ============================================================================
// Degenerate track
// ============================================================================

TEST(SliderTest, ATrackShorterThanItsThumbHasNoTravel)
{
    const thumb_metrics_t metrics = sliderMetrics({ 0.0F, 0.0F, 5.0F, 10.0F }, THUMB);
    EXPECT_FLOAT_EQ(metrics.travel(), 0.0F);
    EXPECT_FLOAT_EQ(thumb_track_t::thumbBound(metrics, 1.0F).x, 0.0F);
    EXPECT_FLOAT_EQ(thumb_track_t::centredValue(metrics, 3.0F), 0.0F);

    // No travel means no value to move, so a drag holds what it grabbed rather
    // than dividing by a zero range
    thumb_track_t drag;
    drag.begin(metrics, 0.0F, 0.5F);
    EXPECT_FLOAT_EQ(drag.valueAt(metrics, 100.0F), 0.5F);
}
