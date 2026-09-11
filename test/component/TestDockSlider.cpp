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
 * @file TestDockSlider.cpp
 * @brief Tests for the dock slider's parts: the element it offers to hit-testing,
 *        the pointer cue that grows the thumb, and the press-and-drag gesture.
 *
 * TestSlider covers the pure geometry - travel, clamping, the two presses. What
 * this adds is the part the res split introduced: the track and the thumb are
 * now sized separately, and the thumb is drawn several times the track's
 * thickness. So the region a pointer can press has to be the thumb's, not the
 * track's, or most of what a reader aims at does nothing.
 *
 * A real ResManager over the shipped res/ supplies the three dock-slider blocks.
 */

#include "ui/render/dockcolumn.h"
#include "ui/render/uielement.h"
#include "ui/render/uilayout.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/row.h"
#include "ui/res/dock/rowkind.h"
#include "ui/res/dock/state.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using Ui::fpx_t;
using Ui::Render::DockColumn;
using Ui::Render::UiElementType;
using Ui::Render::UiLayout;
using Ui::Res::ResManager;
using Ui::Res::Dock::dock_config_t;
using Ui::Res::Dock::dock_state_t;
using Ui::Res::Dock::DockAnchor;
using Ui::Res::Dock::row_t;
using Ui::Res::Dock::RowKind;

namespace {

constexpr fpx_t    DOCK_TOP    = 100.0F;
constexpr fpx_t    DOCK_HEIGHT = 400.0F;
constexpr fpx_t    DOCK_EDGE   = 300.0F; // viewport-facing edge of a left dock
constexpr fpx_t    DOCK_WIDTH  = 180.0F;
constexpr Ui::id_t SLIDER_ROW  = 7;

class DockSliderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        s_resManager = std::make_unique<ResManager>(PROJECT_RES_DIR);
        s_resManager->loadAll();
    }

    static void TearDownTestSuite() { s_resManager.reset(); }

    static dock_config_t seed(const std::string & name)
    {
        s_resManager->setDockState(name, dock_state_t { DOCK_WIDTH, DOCK_WIDTH });
        return dock_config_t { name, DockAnchor::Left, 1, DOCK_WIDTH };
    }

    // One slider row at a known value, with a plain row above it so the slider is
    // never the first thing in the dock - an index of 0 would hide an off-by-one
    static std::vector<row_t> rowsWithSlider(fpx_t ratio)
    {
        std::vector<row_t> rows;
        row_t              text;
        text.id    = 1;
        text.label = "above";
        rows.emplace_back(std::move(text));

        row_t slider;
        slider.id    = SLIDER_ROW;
        slider.label = "Offset";
        slider.kind  = RowKind::Slider;
        slider.ratio = ratio;
        rows.emplace_back(std::move(slider));
        return rows;
    }

    // The slider element the dock offers for hit-testing, if it offered one
    [[nodiscard]] static std::optional<Ui::Res::Type::bound_t> sliderElement(const UiLayout & layout)
    {
        for (const auto & element : layout.elements()) {
            if (element.type == UiElementType::DockSlider) {
                return element.bound;
            }
        }
        return std::nullopt;
    }

    // The same rows in a dock long enough to need a scrollbar. The rows then give
    // up its width, and the slider track moves with them
    static std::vector<row_t> scrollingRowsWithSlider(fpx_t ratio)
    {
        std::vector<row_t> rows = rowsWithSlider(ratio);
        for (Ui::id_t i = 0; i < 40; ++i) {
            row_t filler;
            filler.id    = 100 + i;
            filler.label = "below";
            rows.emplace_back(std::move(filler));
        }
        return rows;
    }

    // Where the thumb's middle sits at `ratio`. The travel is the track less the
    // thumb, because the thumb never overhangs the ends
    [[nodiscard]] static fpx_t thumbCentreX(const Ui::Res::Type::bound_t & track, fpx_t ratio)
    {
        const fpx_t thumb = s_resManager->layout().sliderThumbHoverW;
        return track.x + ((track.w - thumb) * ratio) + (thumb / 2.0F);
    }

    // A dock laid out with one slider row, ready to be asked questions
    [[nodiscard]] static std::unique_ptr<DockColumn> dockWith(const std::string & name, fpx_t ratio)
    {
        auto dock = std::make_unique<DockColumn>(1, seed(name), *s_resManager);
        dock->setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
        dock->setRows(rowsWithSlider(ratio));
        return dock;
    }

    static std::unique_ptr<ResManager> s_resManager;
};

std::unique_ptr<ResManager> DockSliderTest::s_resManager;

} // namespace

// ============================================================================
// The element offered for pressing
// ============================================================================

TEST_F(DockSliderTest, ASliderRowContributesOneElement)
{
    UiLayout layout;
    dockWith("sl-one", 0.5F)->appendElements(layout);
    EXPECT_TRUE(sliderElement(layout).has_value());
}

TEST_F(DockSliderTest, ATextOnlyDockOffersNoSliderElement)
{
    UiLayout   layout;
    DockColumn dock(1, seed("sl-none"), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows({ row_t {} });
    dock.appendElements(layout);
    EXPECT_FALSE(sliderElement(layout).has_value());
}

TEST_F(DockSliderTest, TheHitAreaIsAtLeastAsTallAsTheHoverThumb)
{
    // The defect this replaces: the element was the TRACK's bound, so with a 6px
    // track under an 18px grip two thirds of the visible thumb did nothing
    UiLayout layout;
    dockWith("sl-tall", 0.5F)->appendElements(layout);

    const auto bound = sliderElement(layout);
    ASSERT_TRUE(bound.has_value());
    EXPECT_GE(bound->h, s_resManager->layout().sliderThumbHoverH);
}

TEST_F(DockSliderTest, TheHitAreaIsNoWiderThanTheTrack)
{
    // Length needs no growing - the thumb is kept inside the track ends, so
    // nothing is ever drawn past them and a wider region would swallow presses
    // meant for the value column beside it
    UiLayout layout;
    dockWith("sl-wide", 0.5F)->appendElements(layout);

    const auto bound = sliderElement(layout);
    ASSERT_TRUE(bound.has_value());
    EXPECT_LE(bound->w, DOCK_WIDTH / 2.0F);
}

TEST_F(DockSliderTest, TheHitAreaSitsInsideItsRow)
{
    // It grows about the track's centre, so a thumb taller than the track must
    // not push the pressable region into the rows above or below
    UiLayout layout;
    dockWith("sl-inside", 0.5F)->appendElements(layout);

    const auto  bound = sliderElement(layout);
    const fpx_t rowH  = s_resManager->popup().itemHeight;
    ASSERT_TRUE(bound.has_value());
    // Row index 1, so the row spans [DOCK_TOP + rowH, DOCK_TOP + 2 * rowH]
    EXPECT_GE(bound->y, DOCK_TOP + rowH);
    EXPECT_LE(bound->y + bound->h, DOCK_TOP + (rowH * 2.0F));
}

// ============================================================================
// The hover cue
// ============================================================================

TEST_F(DockSliderTest, PointerOnTheThumbIsAChange)
{
    UiLayout layout;
    auto     dock = dockWith("sl-hot", 0.5F);
    dock->appendElements(layout);

    const auto bound = sliderElement(layout);
    ASSERT_TRUE(bound.has_value());
    // The thumb is centred on the track at 0.5, which is the middle of the hit
    // area on both axes
    EXPECT_TRUE(dock->onMouseMove(bound->x + (bound->w / 2.0F), bound->y + (bound->h / 2.0F)));
}

TEST_F(DockSliderTest, PointerOnBareTrackIsNotOnTheThumb)
{
    UiLayout layout;
    auto     dock = dockWith("sl-bare", 0.0F);
    dock->appendElements(layout);

    const auto bound = sliderElement(layout);
    ASSERT_TRUE(bound.has_value());
    // Value 0 parks the thumb at the left end, so the far right of the track is
    // bare - pressable, but not the grip, and it must not grow anything
    EXPECT_FALSE(dock->onMouseMove(bound->x + bound->w - 1.0F, bound->y + (bound->h / 2.0F)));
}

TEST_F(DockSliderTest, StayingOnTheThumbIsNotAChange)
{
    // The cue drives a redraw, so it must report a TRANSITION and not merely a
    // position - otherwise every pointer move over the dock repaints it
    UiLayout layout;
    auto     dock = dockWith("sl-stay", 0.5F);
    dock->appendElements(layout);

    const auto bound = sliderElement(layout);
    ASSERT_TRUE(bound.has_value());
    const fpx_t centreX = bound->x + (bound->w / 2.0F);
    const fpx_t centreY = bound->y + (bound->h / 2.0F);

    EXPECT_TRUE(dock->onMouseMove(centreX, centreY));
    EXPECT_FALSE(dock->onMouseMove(centreX + 1.0F, centreY));
}

TEST_F(DockSliderTest, LeavingTheThumbIsAChangeBack)
{
    UiLayout layout;
    auto     dock = dockWith("sl-leave", 0.5F);
    dock->appendElements(layout);

    const auto bound = sliderElement(layout);
    ASSERT_TRUE(bound.has_value());
    ASSERT_TRUE(dock->onMouseMove(bound->x + (bound->w / 2.0F), bound->y + (bound->h / 2.0F)));
    EXPECT_TRUE(dock->onMouseMove(bound->x - 50.0F, bound->y - 50.0F));
}

// A scrollbar takes its width out of the rows, so the track moves left with them.
// The cue has to follow the thumb where it is DRAWN: aimed just inside the thumb's
// leading edge, which is where the row rect and the full content rect disagree -
// at its centre both would answer, and the test would prove nothing
TEST_F(DockSliderTest, TheThumbIsHotWhereItIsDrawnInAScrollingDock)
{
    UiLayout   layout;
    DockColumn dock(1, seed("sl-scroll"), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(scrollingRowsWithSlider(1.0F));
    dock.appendElements(layout);
    ASSERT_TRUE(dock.isScrollable());

    const auto track = sliderElement(layout);
    ASSERT_TRUE(track.has_value());
    const fpx_t thumb = s_resManager->layout().sliderThumbHoverW;
    EXPECT_TRUE(dock.onMouseMove(track->x + track->w - thumb + 1.0F, track->y + (track->h / 2.0F)));
}

// ============================================================================
// The gesture
// ============================================================================

// A press on the thumb takes hold of it where it is: no new value for the host,
// and the drag starts from the value it already had
TEST_F(DockSliderTest, PressingTheThumbGrabsItInPlace)
{
    UiLayout layout;
    auto     dock = dockWith("sl-grab", 0.5F);
    dock->appendElements(layout);

    const auto track = sliderElement(layout);
    ASSERT_TRUE(track.has_value());
    const fpx_t x = thumbCentreX(*track, 0.5F);
    EXPECT_FALSE(dock->beginSliderGesture(SLIDER_ROW, x).has_value());
    EXPECT_NEAR(dock->onSliderDrag(x), 0.5F, 1e-4F);
}

// A press on bare track jumps the thumb to sit centred under the pointer, reports
// that value, and the drag carries on from it rather than snapping back
TEST_F(DockSliderTest, PressingBareTrackJumpsTheThumbUnderThePointer)
{
    UiLayout layout;
    auto     dock = dockWith("sl-jump", 0.0F);
    dock->appendElements(layout);

    const auto track = sliderElement(layout);
    ASSERT_TRUE(track.has_value());
    const fpx_t                x      = thumbCentreX(*track, 0.75F);
    const std::optional<fpx_t> jumped = dock->beginSliderGesture(SLIDER_ROW, x);
    ASSERT_TRUE(jumped.has_value());
    EXPECT_NEAR(*jumped, 0.75F, 1e-4F);
    EXPECT_NEAR(dock->onSliderDrag(x), 0.75F, 1e-4F);
}

// The thumb stays under the pointer - a move is worth the same share of the travel
// as the thumb covers - and past either end the value holds at the limit
TEST_F(DockSliderTest, DraggingKeepsTheThumbUnderThePointerAndClamps)
{
    UiLayout layout;
    auto     dock = dockWith("sl-drag", 0.5F);
    dock->appendElements(layout);

    const auto track = sliderElement(layout);
    ASSERT_TRUE(track.has_value());
    ASSERT_FALSE(dock->beginSliderGesture(SLIDER_ROW, thumbCentreX(*track, 0.5F)).has_value());
    EXPECT_NEAR(dock->onSliderDrag(thumbCentreX(*track, 0.75F)), 0.75F, 1e-4F);
    EXPECT_FLOAT_EQ(dock->onSliderDrag(track->x + track->w + 50.0F), 1.0F);
    EXPECT_FLOAT_EQ(dock->onSliderDrag(track->x - 50.0F), 0.0F);
}

// Only a slider row this dock holds is its to drag: a row id from elsewhere, and
// a text row it does hold, both have no thumb here
TEST_F(DockSliderTest, OnlyThisDocksSliderRowsAreOffered)
{
    auto dock = dockWith("sl-owner", 0.5F);
    EXPECT_TRUE(dock->hasSliderRow(SLIDER_ROW));
    EXPECT_FALSE(dock->hasSliderRow(SLIDER_ROW + 1));
    EXPECT_FALSE(dock->hasSliderRow(1));
}
