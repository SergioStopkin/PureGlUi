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
 * @file TestPopupPlacement.cpp
 * @brief Where a menu's list stands on screen, held against what the OS itself says
 *
 * A list is a window of its own, placed from where the framework thinks the main window's
 * content starts and then cut to its shape. Both are each platform's own code - on macOS
 * contentRectForFrameRect, the point flip of a popup's frame and its corner mask - so every
 * case asks the OS directly (nativegeometry.h), windowed and again in the largest state the
 * OS gives a window: macOS's fullscreen is where a list stood a point below its button
 *
 * Skipped where a list is no window to ask about: native Wayland, which has no global
 * coordinates, and composited X11, where a list is drawn into the main window
 */

#if defined(_WIN32) || defined(__APPLE__) || defined(HAVE_X11)

#include "menubarloop.h"
#include "nativegeometry.h"
#include "testconst.h"
#include "ui/color.h"
#include "ui/config.h"
#include "ui/interface/iwindow.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/dialog.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/nativewindow.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/popup/popupwindow.h"

#include <cmath>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

namespace {

using PureGlUi::PLACEMENT_DEBUG;

// The first pixel an edge at css px reaches: GL fills a pixel whose centre is inside
Ui::fpx_t firstPixelFrom(Ui::fpx_t css) { return std::ceil(Ui::toPhys(css) - 0.5F); }

// Where the OS has a popup at a step of putting it on screen
void tracePlacement(const Ui::IWindow & popup, const char * step)
{
    if constexpr (PLACEMENT_DEBUG) {
        std::cout << "[Placement] " << step << ": y=" << PureGlUi::nativeContentRectOf(popup).y << std::endl;
    }
}

class PopupPlacementTest : public ::testing::TestWithParam<bool> {
protected:
    Ui::Shell m_shell;
    bool      m_isInitialized = false;
    bool      m_isEnlarged    = false;

    void SetUp() override
    {
        if (!m_shell.initialize()) {
            GTEST_SKIP() << "no window or GL context here";
        }
        m_isInitialized = true;
        if (Ui::g_config.isCompositing) {
            GTEST_SKIP() << "composited: a list is drawn into the main window, not placed as a window of its own";
        }
        if (GetParam()) {
            m_isEnlarged = PureGlUi::setEnlarged(mainWindow(), true);
            if (!m_isEnlarged) {
                GTEST_SKIP() << "the OS gave the window no larger state here";
            }
        }
        // The frame the app draws before anyone can click a menu
        m_shell.windowManager().renderAll();
    }

    void TearDown() override
    {
        if (m_isEnlarged) {
            (void)PureGlUi::setEnlarged(mainWindow(), false);
        }
        if (m_isInitialized) {
            m_shell.shutdown();
        }
    }

    [[nodiscard]] Ui::Window::NativeWindow & mainWindow() { return m_shell.windowManager().mainWindow(); }

    // The list a click on the menu opens, once the OS has placed it
    [[nodiscard]] Ui::Window::Popup::PopupWindow * openList(Ui::id_t menuId)
    {
        m_shell.openPopup(menuId);
        Ui::Window::Popup::PopupWindow * list = m_shell.windowManager().popupWindow();
        if (list != nullptr) {
            PureGlUi::settle(*list);
        }
        return list;
    }
};

// Every popup is placed from this corner, so where the main window's content really
// stands is the first thing a platform must answer right
TEST_P(PopupPlacementTest, TheMainWindowSaysWhereItsContentIs)
{
    int x {};
    int y {};
    mainWindow().screenPosition(x, y);

    const Ui::Res::Type::bound_t content = PureGlUi::nativeContentRectOf(mainWindow());
    EXPECT_FLOAT_EQ(static_cast<Ui::fpx_t>(x), content.x);
    EXPECT_FLOAT_EQ(static_cast<Ui::fpx_t>(y), content.y);
}

// A popup asked for at a rect is made there: the platform's own conversion, the flip of a
// macOS frame into points, gives back what it was handed - at the content's very top too,
// which in fullscreen is the screen's, where an OS may keep windows clear of its menu bar.
// Whole px at any scale, as the framework hands them (toPhysEdge), so no platform has a
// fraction of one to round. Shown, it is a menu's list, which
// EveryMenuListStandsFlushUnderItsButton opens as a click does
TEST_P(PopupPlacementTest, APopupLandsWhereItIsAsked)
{
    const Ui::Res::Type::bound_t asked { .x = Ui::toPhysEdge(40.0F),
                                         .y = 0.0F,
                                         .w = Ui::toPhysEdge(160.0F),
                                         .h = Ui::toPhysEdge(120.0F) };
    ASSERT_TRUE(m_shell.windowManager().createPopup(asked, {}, Ui::Color {}));
    Ui::Window::Popup::PopupWindow * popup = m_shell.windowManager().popupWindow();
    ASSERT_NE(popup, nullptr);
    tracePlacement(*popup, "made");
    PureGlUi::settle(*popup);
    tracePlacement(*popup, "settled");

    const Ui::Res::Type::bound_t main   = PureGlUi::nativeContentRectOf(mainWindow());
    const Ui::Res::Type::bound_t landed = PureGlUi::nativeContentRectOf(*popup);
    EXPECT_FLOAT_EQ(landed.x - main.x, asked.x);
    EXPECT_FLOAT_EQ(landed.y - main.y, asked.y);
    EXPECT_FLOAT_EQ(landed.w, asked.w);
    EXPECT_FLOAT_EQ(landed.h, asked.h);
}

// A list a click opens is on screen, starting on the pixel where its button's left edge is
// drawn and right under its bottom, for every menu: a middle one's x is the sum of the text
// widths before it, a fraction a list rounded otherwise stands a pixel off
TEST_P(PopupPlacementTest, EveryMenuListStandsFlushUnderItsButton)
{
    const std::vector<Ui::id_t> menuIds = PureGlUi::listMenuIdsOf(m_shell);
    ASSERT_FALSE(menuIds.empty());

    for (const Ui::id_t menuId : menuIds) {
        const Ui::Window::Popup::PopupWindow * list = openList(menuId);
        ASSERT_NE(list, nullptr) << "menu " << menuId;
        EXPECT_TRUE(PureGlUi::isOnScreen(*list)) << "menu " << menuId << ": its list is not on screen";

        const Ui::Res::Type::bound_t & button = m_shell.windowManager().elementBound(menuId);
        const Ui::Res::Type::bound_t   main   = PureGlUi::nativeContentRectOf(mainWindow());
        const Ui::Res::Type::bound_t   shown  = PureGlUi::nativeContentRectOf(*list);
        EXPECT_FLOAT_EQ(shown.x - main.x, firstPixelFrom(button.x)) << "menu " << menuId << ": its list's left edge";
        EXPECT_FLOAT_EQ(shown.y - main.y, firstPixelFrom(button.y + button.h))
        << "menu " << menuId << ": its list's top";

        m_shell.closePopup();
    }
}

// A dialog, centred in the area inside the chrome, stands on a whole CSS px: at macOS's 2x
// half a point leaves its mask, snapped to whole points, a point off what GL draws - a black
// line along its top. Centring halves the room round it, so two heights a CSS px apart
// give one odd half whatever the window's size
TEST_P(PopupPlacementTest, EveryDialogStandsOnAWholeCssPx)
{
    for (const Ui::fpx_t height : { 200.0F, 201.0F }) {
        Ui::Res::Type::dialog_t dialog;
        dialog.title  = "placement";
        dialog.width  = 300.0F;
        dialog.height = height;
        m_shell.windowManager().openDialog(dialog);
        Ui::Window::Popup::DialogWindow * shown = m_shell.windowManager().dialogWindow();
        ASSERT_NE(shown, nullptr) << height << " CSS px high";
        PureGlUi::settle(*shown);

        const Ui::Res::Type::bound_t main   = PureGlUi::nativeContentRectOf(mainWindow());
        const Ui::Res::Type::bound_t placed = PureGlUi::nativeContentRectOf(*shown);
        const Ui::fpx_t              left   = placed.x - main.x;
        const Ui::fpx_t              top    = placed.y - main.y;
        EXPECT_FLOAT_EQ(left, firstPixelFrom(std::round(Ui::toCss(left)))) << height << " CSS px high: its left edge";
        EXPECT_FLOAT_EQ(top, firstPixelFrom(std::round(Ui::toCss(top)))) << height << " CSS px high: its top";

        // The window the OS made spans the pixels between its edges - the size it was asked
        // wherever that is whole pixels - and shows its whole top line between its corners:
        // a taller one is a strip GL never draws, black over the top, a shorter one cuts GL's
        const Ui::fpx_t leftCss = std::round(Ui::toCss(left));
        const Ui::fpx_t topCss  = std::round(Ui::toCss(top));
        EXPECT_FLOAT_EQ(placed.w, firstPixelFrom(leftCss + dialog.width) - firstPixelFrom(leftCss))
        << height << " CSS px high: its width";
        EXPECT_FLOAT_EQ(placed.h, firstPixelFrom(topCss + dialog.height) - firstPixelFrom(topCss))
        << height << " CSS px high: its height";
        const Ui::Res::Type::border_t & radii       = shown->cornerRadii();
        int                             firstHidden = -1;
        for (int x = static_cast<int>(std::ceil(radii.topLeft)); x < placed.w - radii.topRight && firstHidden < 0;
             ++x) {
            if (!PureGlUi::isShownAt(*shown, x, 0)) {
                firstHidden = x;
            }
        }
        EXPECT_EQ(firstHidden, -1) << height << " CSS px high: its top line is cut from x " << firstHidden;

        m_shell.windowManager().closeDialog();
    }
}

// The shape a list is cut to leaves its top line whole between its corners, so nothing
// under it shows through where the list meets its button
TEST_P(PopupPlacementTest, AMenuListShowsItsWholeTopLine)
{
    const std::vector<Ui::id_t> menuIds = PureGlUi::listMenuIdsOf(m_shell);
    ASSERT_FALSE(menuIds.empty());
    const Ui::Window::Popup::PopupWindow * list = openList(menuIds.front());
    ASSERT_NE(list, nullptr);

    const Ui::Res::Type::border_t & radii       = list->cornerRadii();
    const Ui::Res::Type::bound_t    shown       = PureGlUi::nativeContentRectOf(*list);
    int                             firstHidden = -1;
    for (int x = static_cast<int>(std::ceil(radii.topLeft)); x < shown.w - radii.topRight && firstHidden < 0; ++x) {
        if (!PureGlUi::isShownAt(*list, x, 0)) {
            firstHidden = x;
        }
    }
    EXPECT_EQ(firstHidden, -1) << "the list's top line is cut from x " << firstHidden;
}

INSTANTIATE_TEST_SUITE_P(Window,
                         PopupPlacementTest,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool> & info) {
                             return std::string { info.param ? "Enlarged" : "Windowed" };
                         });

} // namespace

#endif
