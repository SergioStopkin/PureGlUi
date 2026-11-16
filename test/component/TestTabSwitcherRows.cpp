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
 * @file TestTabSwitcherRows.cpp
 * @brief Unit tests for UiLayout::fitTabSwitcherRows, the Ctrl+Tab list's rows fitted to the
 *        window: a row its label, a gap and its description between its paddings; the list at
 *        least tab-switcher's width and never wider than the room it is given; a description cut
 *        before its label, a label keeping its extension, and nothing kept where nothing fits.
 *        Measured by FakeRender's fixed-width glyphs, so every width and cut is exact
 */

#include "common/unicode.h"
#include "fakerender.h"
#include "testconst.h"
#include "ui/codepoint.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/tab.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

namespace {

    constexpr Ui::fpx_t PAD   = 16.0F;  // the shipped tab-switcher-item padding
    constexpr Ui::fpx_t BLOCK = 160.0F; // the shipped tab-switcher width

    // `count` glyphs of `letter`, GLYPH_WIDTH each
    std::string glyphs(int count, char letter) { return std::string(static_cast<std::size_t>(count), letter); }

    Ui::tab_t rowOf(const std::string & label, const std::string & description)
    {
        return Ui::tab_t { .id = 1, .label = label, .description = description };
    }

} // namespace

class TabSwitcherRowsTest : public ::testing::Test {
protected:
    Ui::Res::ResManager  resManager { TEST_RES_DIR };
    FakeRender           render;
    Ui::Render::UiLayout layout;
    const std::string    ellipsis = Common::Unicode::toUtf8(Ui::wstr(Ui::Codepoint::ThreeDotH));

    void SetUp() override
    {
        resManager.loadAll();
        ASSERT_FLOAT_EQ(resManager.layout().tabSwitcherItem.padding, PAD) << "the shipped padding moved";
        ASSERT_FLOAT_EQ(resManager.layout().tabSwitcher.width, BLOCK) << "the shipped width moved";
        layout.build(resManager.layout(),
                     resManager.theme(),
                     resManager.menus(),
                     resManager.buttons(),
                     resManager.tabBar(),
                     resManager.localeManager(),
                     WINDOW_W,
                     WINDOW_H,
                     &render);
    }

    // `rows` fitted to `maxWidth`, and the width they take
    [[nodiscard]] Ui::fpx_t fit(Ui::fpx_t maxWidth, std::vector<Ui::tab_t> & rows)
    {
        return layout.fitTabSwitcherRows(maxWidth, resManager, render, rows);
    }
};

TEST_F(TabSwitcherRowsTest, NoRowsAreTheBlocksWidth)
{
    std::vector<Ui::tab_t> rows;
    EXPECT_FLOAT_EQ(fit(800.0F, rows), BLOCK);
}

TEST_F(TabSwitcherRowsTest, NoRowsInANarrowWindowAreTheWindowsWidth)
{
    std::vector<Ui::tab_t> rows;
    EXPECT_FLOAT_EQ(fit(100.0F, rows), 100.0F);
}

TEST_F(TabSwitcherRowsTest, AShortRowIsTheBlocksWidthUnchanged)
{
    std::vector<Ui::tab_t> rows { rowOf("a.step", "") };
    EXPECT_FLOAT_EQ(fit(800.0F, rows), BLOCK);
    EXPECT_EQ(rows.at(0).label, "a.step");
    EXPECT_EQ(rows.at(0).description, "");
}

// A label wider than the block sets the width, its paddings round it and no gap after it
TEST_F(TabSwitcherRowsTest, NoDescriptionAddsNoGap)
{
    std::vector<Ui::tab_t> rows { rowOf(glyphs(20, 'l'), "") };
    EXPECT_FLOAT_EQ(fit(800.0F, rows), PAD + (20 * GLYPH_WIDTH) + PAD);
}

TEST_F(TabSwitcherRowsTest, ADescriptionAddsAGapAndItsWidth)
{
    std::vector<Ui::tab_t> rows { rowOf(glyphs(10, 'l'), glyphs(10, 'd')) };
    EXPECT_FLOAT_EQ(fit(800.0F, rows), PAD + (10 * GLYPH_WIDTH) + PAD + (10 * GLYPH_WIDTH) + PAD);
    EXPECT_EQ(rows.at(0).label, glyphs(10, 'l'));
    EXPECT_EQ(rows.at(0).description, glyphs(10, 'd'));
}

TEST_F(TabSwitcherRowsTest, TheWidestRowSetsTheWidth)
{
    std::vector<Ui::tab_t> rows { rowOf(glyphs(5, 'a'), ""), rowOf(glyphs(30, 'b'), "") };
    EXPECT_FLOAT_EQ(fit(800.0F, rows), PAD + (30 * GLYPH_WIDTH) + PAD);
    EXPECT_EQ(rows.at(0).label, glyphs(5, 'a'));
}

// 200 px: 168 for a label between its paddings, a 21-glyph one exactly - kept whole, and no room
// left for a description, which goes
TEST_F(TabSwitcherRowsTest, ALabelThatJustFitsIsKeptItsDescriptionDropped)
{
    std::vector<Ui::tab_t> rows { rowOf(glyphs(21, 'l'), "folder") };
    EXPECT_FLOAT_EQ(fit(200.0F, rows), 200.0F);
    EXPECT_EQ(rows.at(0).label, glyphs(21, 'l'));
    EXPECT_EQ(rows.at(0).description, "");
}

// 200 px with a 10-glyph label: 72 px left for the description after three paddings, eight glyphs
// and the ellipsis - the label untouched
TEST_F(TabSwitcherRowsTest, ADescriptionIsCutBeforeItsLabel)
{
    std::vector<Ui::tab_t> rows { rowOf(glyphs(10, 'l'), glyphs(30, 'd')) };
    EXPECT_FLOAT_EQ(fit(200.0F, rows), 200.0F);
    EXPECT_EQ(rows.at(0).label, glyphs(10, 'l'));
    EXPECT_EQ(rows.at(0).description, glyphs(8, 'd') + ellipsis);
}

// A file's name too long for 168 px keeps its extension: 16 glyphs, the ellipsis and "step"
TEST_F(TabSwitcherRowsTest, ALongLabelKeepsItsExtension)
{
    std::vector<Ui::tab_t> rows { rowOf("averyverylongfilename.step", "folder") };
    EXPECT_FLOAT_EQ(fit(200.0F, rows), 200.0F);
    EXPECT_EQ(rows.at(0).label, "averyverylongfil" + ellipsis + "step");
    EXPECT_EQ(rows.at(0).description, "");
}

// No extension to keep: as many glyphs as leave room for the ellipsis, 268 px holding 32 of them
TEST_F(TabSwitcherRowsTest, ALongLabelWithNoExtensionIsCut)
{
    std::vector<Ui::tab_t> rows { rowOf(glyphs(50, 'x'), "folder") };
    const Ui::fpx_t        width = fit(300.0F, rows);
    EXPECT_EQ(rows.at(0).label, glyphs(32, 'x') + ellipsis);
    EXPECT_FLOAT_EQ(width, PAD + (33 * GLYPH_WIDTH) + PAD);
}

TEST_F(TabSwitcherRowsTest, NeverWiderThanTheRoomGiven)
{
    std::vector<Ui::tab_t> rows { rowOf(glyphs(80, 'l'), glyphs(80, 'd')), rowOf(glyphs(60, 'm'), "") };
    EXPECT_LE(fit(300.0F, rows), 300.0F);
}

// Each row is fitted on its own: a short one beside a long one keeps all it has
TEST_F(TabSwitcherRowsTest, EachRowIsFittedOnItsOwn)
{
    std::vector<Ui::tab_t> rows { rowOf("short.step", "dir"), rowOf(glyphs(30, 'l'), glyphs(30, 'd')) };
    EXPECT_FLOAT_EQ(fit(200.0F, rows), 200.0F);
    EXPECT_EQ(rows.at(0).label, "short.step");
    EXPECT_EQ(rows.at(0).description, "dir");
    EXPECT_EQ(rows.at(1).label, glyphs(20, 'l') + ellipsis);
    EXPECT_EQ(rows.at(1).description, "");
}

// Room for not even its paddings: no text at all, and the list as wide as the room
TEST_F(TabSwitcherRowsTest, AWindowTooNarrowForARowKeepsNoText)
{
    std::vector<Ui::tab_t> rows { rowOf("a.step", "folder") };
    EXPECT_FLOAT_EQ(fit(30.0F, rows), 30.0F);
    EXPECT_EQ(rows.at(0).label, "");
    EXPECT_EQ(rows.at(0).description, "");
}

} // namespace PureGlUi
