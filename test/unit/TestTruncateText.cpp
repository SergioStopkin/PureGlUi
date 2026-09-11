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
 * @file TestTruncateText.cpp
 * @brief Unit tests for the two truncators in ui/render/truncate.h: the plain
 *        end-truncation dock rows use, and the filename ladder tabs use. The
 *        pair is what makes the distinction testable - a measured value must not
 *        be split at its decimal point the way a filename is at its extension.
 *
 * Text is measured through a fixed-width fake sink, so a character is worth a
 * known number of pixels and every expectation below is exact.
 */

#include "ui/interface/irender.h"
#include "ui/render/truncate.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>

using Ui::fpx_t;
using Ui::Render::truncateFileName;
using Ui::Render::truncateText;

namespace {

// Every glyph is one unit wide, so "abcd" measures 4 and the arithmetic in the
// expectations below is the same a reader can do by hand. The ellipsis is a
// glyph like any other, so it also costs 1.
constexpr fpx_t GLYPH = 1.0F;

class MeasuringRender final : public Ui::IRender {
public:
    void beginFrame(fpx_t /*width*/, fpx_t /*height*/) override { }
    void endFrame() override { }
    void fillRect(const Ui::Res::Type::bound_t & /*bound*/,
                  const Ui::Res::Type::border_t & /*radii*/,
                  const Ui::Res::Type::color_pair_t & /*colors*/,
                  const Ui::Render::shadow_t & /*shadow*/) override
    {
    }
    void drawText(Ui::font_handle_t /*font*/,
                  std::string_view /*text*/,
                  const Ui::Res::Type::bound_t & /*pos*/,
                  const Ui::Color & /*color*/,
                  Ui::Res::Type::AlignH /*alignH*/,
                  Ui::Res::Type::AlignV /*alignV*/,
                  fpx_t /*minPadH*/) override
    {
    }
    void drawImage(std::string_view /*src*/,
                   const Ui::Res::Type::bound_t & /*bound*/,
                   const Ui::Res::Type::border_t & /*radii*/,
                   const Ui::Color & /*tint*/,
                   fpx_t /*scale*/,
                   const Ui::Render::shadow_t & /*shadow*/,
                   bool /*isFilled*/) override
    {
    }
    void warmImage(std::string_view /*src*/,
                   const Ui::Res::Type::bound_t & /*bound*/,
                   const Ui::Color & /*tint*/,
                   fpx_t /*scale*/,
                   bool /*isFilled*/) override
    {
    }
    void drawTriangle(fpx_t /*x0*/,
                      fpx_t /*y0*/,
                      fpx_t /*x1*/,
                      fpx_t /*y1*/,
                      fpx_t /*x2*/,
                      fpx_t /*y2*/,
                      const Ui::Color & /*color*/) override
    {
    }

    [[nodiscard]] fpx_t textWidth(Ui::font_handle_t /*font*/, std::wstring_view text) override
    {
        return static_cast<fpx_t>(text.size()) * GLYPH;
    }

    [[nodiscard]] Ui::font_handle_t createFont(const Ui::Res::Type::font_t & /*font*/) override { return 1; }
};

constexpr Ui::font_handle_t FONT = 1;

std::string cut(const std::string & text, fpx_t maxW)
{
    MeasuringRender render;
    return truncateText(text, maxW, &render, FONT);
}

std::string cutFile(const std::string & text, fpx_t maxW)
{
    MeasuringRender render;
    return truncateFileName(text, maxW, &render, FONT);
}

} // namespace

// ============================================================================
// Positive: it fits, or it is cut to fit
// ============================================================================

TEST(TruncateText, TextThatFitsIsUntouched)
{
    EXPECT_EQ(cut("Area", 4.0F), "Area");
    EXPECT_EQ(cut("Area", 40.0F), "Area");
}

TEST(TruncateText, TextThatDoesNotFitKeepsAPrefixAndTheEllipsis)
{
    // 3 units: two glyphs of prefix plus the one-glyph ellipsis
    const std::string text = cut("Total area", 3.0F);
    EXPECT_TRUE(text.starts_with("To"));
    EXPECT_FALSE(text.starts_with("Tot"));
}

// The reason this function exists next to the filename one
TEST(TruncateText, ANumberIsCutAtItsEndNotSplitAtTheDecimalPoint)
{
    const std::string text = cut("0.0853034 mm2", 4.0F);
    EXPECT_TRUE(text.starts_with("0.0"));
    EXPECT_EQ(text.find("mm2"), std::string::npos);
}

// ============================================================================
// Negative: nothing fits, or there is nothing to measure with
// ============================================================================

TEST(TruncateText, ZeroWidthYieldsNothing) { EXPECT_EQ(cut("Area", 0.0F), ""); }

TEST(TruncateText, NegativeWidthYieldsNothing) { EXPECT_EQ(cut("Area", -10.0F), ""); }

TEST(TruncateText, WidthUnderOneGlyphCollapsesToTheEllipsisAlone)
{
    // One unit fits the ellipsis but no prefix; below that, nothing at all
    EXPECT_FALSE(cut("Area", 1.0F).empty());
    EXPECT_EQ(cut("Area", 0.5F), "");
}

TEST(TruncateText, NoRenderOrNoFontReturnsTheTextUnchanged)
{
    MeasuringRender render;
    EXPECT_EQ(truncateText("Area", 1.0F, nullptr, FONT), "Area");
    EXPECT_EQ(truncateText("Area", 1.0F, &render, 0), "Area");
}

TEST(TruncateText, EmptyTextStaysEmpty) { EXPECT_EQ(cut("", 10.0F), ""); }

// ============================================================================
// The filename ladder, for contrast
// ============================================================================

TEST(TruncateFileName, KeepsTheExtensionWhenThereIsRoomForIt)
{
    // "model.stp" is 9; at 7 the ladder drops name characters but keeps "stp"
    const std::string text = cutFile("model.stp", 7.0F);
    EXPECT_TRUE(text.ends_with("stp"));
}

TEST(TruncateFileName, DropsTheExtensionRatherThanTheWholeName)
{
    const std::string text = cutFile("model.stp", 3.0F);
    EXPECT_FALSE(text.empty());
    EXPECT_FALSE(text.ends_with("stp"));
}

// A leading dot is part of the name, not an extension marker
TEST(TruncateFileName, ADotFileIsNotSplitAtItsLeadingDot)
{
    const std::string text = cutFile(".hidden", 4.0F);
    EXPECT_TRUE(text.starts_with("."));
}

TEST(TruncateFileName, NoRenderOrNoFontReturnsTheTextUnchanged)
{
    MeasuringRender render;
    EXPECT_EQ(truncateFileName("model.stp", 1.0F, nullptr, FONT), "model.stp");
    EXPECT_EQ(truncateFileName("model.stp", 1.0F, &render, 0), "model.stp");
}
