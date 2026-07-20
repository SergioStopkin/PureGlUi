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
 * @file TestResKeys.cpp
 * @brief Unit tests for the res-key enum -> spelling mappers (Ui::Res::Key).
 *        These enums single-source every JSON key spelling the layout / theme /
 *        section / menu / dialog loaders reference. The tests exercise EVERY
 *        enumerator (full coverage of each KeyName switch), assert no mapper
 *        returns an empty string, assert spellings are unique within an enum,
 *        and spot-check the exact CSS-convention spellings that the shipped res
 *        files depend on.
 */

#include "ui/res/key/cssprop.h"
#include "ui/res/key/dialog.h"
#include "ui/res/key/element.h"
#include "ui/res/key/layout.h"
#include "ui/res/key/menu.h"
#include "ui/res/key/section.h"
#include "ui/res/key/theme.h"

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

using namespace Ui::Res::Key;

namespace {

// Assert every mapped name is non-empty and the whole set is collision-free.
template <typename Key, typename Fn>
void expectNonEmptyAndUnique(const std::vector<Key> & keys, Fn nameOf)
{
    std::set<std::string> seen;
    for (const Key key : keys) {
        const std::string name = nameOf(key);
        EXPECT_FALSE(name.empty()) << "mapper returned empty for enumerator " << static_cast<int>(key);
        EXPECT_TRUE(seen.insert(name).second) << "duplicate spelling: " << name;
    }
    EXPECT_EQ(seen.size(), keys.size());
}

} // namespace

TEST(ResKeys, ElementKeyAllMappedUniqueNonEmpty)
{
    const std::vector<ElementKey> all {
        ElementKey::Root,
        ElementKey::TopMenu,
        ElementKey::TopMenuDropdown,
        ElementKey::TopMenuItem,
        ElementKey::TopMenuItemHover,
        ElementKey::TopMenuItemActive,
        ElementKey::TopMenuItemDisabled,
        ElementKey::TopMenuItemIcon,
        ElementKey::TopMenuItemShortcut,
        ElementKey::TopMenuItemShortcutHover,
        ElementKey::TopMenuSeparator,
        ElementKey::TopMenuButtonLabel,
        ElementKey::TopMenuButtonLabelHover,
        ElementKey::TopMenuButtonLabelActive,
        ElementKey::LeftToolbar,
        ElementKey::RightToolbar,
        ElementKey::StatusBar,
        ElementKey::StatusBarActive,
        ElementKey::Workspace,
        ElementKey::WorkspaceTabs,
        ElementKey::WorkspaceTab,
        ElementKey::WorkspaceTabHover,
        ElementKey::WorkspaceTabActive,
        ElementKey::WorkspaceTabClose,
        ElementKey::WorkspaceTabCloseHover,
        ElementKey::WorkspaceTabArrow,
        ElementKey::Button,
        ElementKey::ThemePreview,
        ElementKey::Dialog,
        ElementKey::DialogTitle,
        ElementKey::DialogText,
        ElementKey::DialogIcon,
        ElementKey::DialogClose,
        ElementKey::DialogCloseHover,
        ElementKey::DialogCloseActive,
        ElementKey::DialogButton,
        ElementKey::DialogButtonHover,
        ElementKey::DialogButtonActive,
        ElementKey::DialogButtonPrimary,
        ElementKey::DialogLink,
        ElementKey::DialogLinkVisited,
        ElementKey::DialogScrollbar,
        ElementKey::DialogScrollbarHover,
        ElementKey::DialogScrollbarThumb,
        ElementKey::DialogScrollbarThumbHover,
        ElementKey::Dock,
        ElementKey::DockDefaults,
        ElementKey::DockGrip,
        ElementKey::DockGripHover,
        ElementKey::DockGripActive,
        ElementKey::DockSeparator,
    };
    expectNonEmptyAndUnique(all, elementKeyName);
    EXPECT_EQ(all.size(), 51U);
}

TEST(ResKeys, ElementKeySpotSpellings)
{
    EXPECT_EQ(elementKeyName(ElementKey::Root), ":root");
    EXPECT_EQ(elementKeyName(ElementKey::TopMenuItemHover), "top-menu-item:hover");
    EXPECT_EQ(elementKeyName(ElementKey::DialogButtonPrimary), "dialog-button:primary");
    EXPECT_EQ(elementKeyName(ElementKey::WorkspaceTabCloseHover), "workspace-tab-close:hover");
    EXPECT_EQ(elementKeyName(ElementKey::DockGripActive), "dock-grip:active");
}

TEST(ResKeys, CssPropKeyAllMappedUniqueNonEmpty)
{
    const std::vector<CssPropKey> all {
        CssPropKey::Color,      CssPropKey::Background,     CssPropKey::Width,          CssPropKey::Height,
        CssPropKey::Margin,     CssPropKey::MarginBottom,   CssPropKey::Padding,        CssPropKey::BorderRadius,
        CssPropKey::Top,        CssPropKey::Left,           CssPropKey::Right,          CssPropKey::Bottom,
        CssPropKey::MinWidth,   CssPropKey::LineHeight,     CssPropKey::FontFamily,     CssPropKey::FontSize,
        CssPropKey::FontWeight, CssPropKey::Icon,           CssPropKey::IconLeft,       CssPropKey::IconRight,
        CssPropKey::Shift,      CssPropKey::SplitAngle,     CssPropKey::MinThumbHeight, CssPropKey::GripWidth,
        CssPropKey::GripIcon,   CssPropKey::ClickThreshold,
    };
    expectNonEmptyAndUnique(all, cssPropKeyName);
    EXPECT_EQ(all.size(), 26U);
}

TEST(ResKeys, CssPropKeySpotSpellings)
{
    EXPECT_EQ(cssPropKeyName(CssPropKey::BorderRadius), "border-radius");
    EXPECT_EQ(cssPropKeyName(CssPropKey::FontFamily), "font-family");
    EXPECT_EQ(cssPropKeyName(CssPropKey::MinThumbHeight), "min-thumb-height");
    EXPECT_EQ(cssPropKeyName(CssPropKey::MarginBottom), "margin-bottom");
}

TEST(ResKeys, LayoutKeyAllMappedUniqueNonEmpty)
{
    const std::vector<LayoutKey> all {
        LayoutKey::ButtonImgSize,          LayoutKey::WindowWidth,
        LayoutKey::WindowHeight,           LayoutKey::ButtonIconHoverShadowX,
        LayoutKey::ButtonIconHoverShadowY, LayoutKey::ButtonIconHoverShadowBlur,
        LayoutKey::ButtonIconActiveScale,  LayoutKey::FallbackCharWidth,
        LayoutKey::MenuMaxDepth,
    };
    expectNonEmptyAndUnique(all, layoutKeyName);
    EXPECT_EQ(all.size(), 9U);
    // Layout variables are CSS custom properties: every spelling is a --var.
    for (const LayoutKey key : all) {
        EXPECT_EQ(layoutKeyName(key).rfind("--", 0), 0U) << layoutKeyName(key);
    }
}

TEST(ResKeys, ThemeKeyAllMappedUniqueNonEmpty)
{
    const std::vector<ThemeKey> all {
        ThemeKey::Name,     ThemeKey::ClMain,        ThemeKey::BgMain,   ThemeKey::ClSecond, ThemeKey::BgSecond,
        ThemeKey::ClShadow, ThemeKey::ShadowOpacity, ThemeKey::ClModel,  ThemeKey::ClError,  ThemeKey::ClLoad,
        ThemeKey::ClInfo,   ThemeKey::ClWarn,        ThemeKey::FontSans, ThemeKey::FontMono,
    };
    expectNonEmptyAndUnique(all, themeKeyName);
    EXPECT_EQ(all.size(), 14U);
    // "name" is a plain field; every other theme key is a --var.
    EXPECT_EQ(themeKeyName(ThemeKey::Name), "name");
    EXPECT_EQ(themeKeyName(ThemeKey::ClMain), "--cl-main");
    EXPECT_EQ(themeKeyName(ThemeKey::BgMain), "--bg-main");
}

// The existing section / menu / dialog mappers, exercised for completeness so
// the whole ui/res/key family is under test in one place.
TEST(ResKeys, SectionKeySpellings)
{
    EXPECT_EQ(sectionKeyName(SectionKey::MainWindow), "mainWindow");
    EXPECT_EQ(sectionKeyName(SectionKey::View), "view");
    EXPECT_EQ(sectionKeyName(SectionKey::Paths), "paths");
    EXPECT_EQ(sectionKeyName(SectionKey::Files), "files");
    EXPECT_EQ(sectionKeyName(SectionKey::Docks), "docks");
}

TEST(ResKeys, MenuAndDialogKeySpellings)
{
    EXPECT_EQ(menuKeyName(MenuKey::Action), "action");
    EXPECT_EQ(menuKeyName(MenuKey::Submenus), "submenus");
    EXPECT_EQ(dialogKeyName(DialogKey::Buttons), "buttons");
    EXPECT_EQ(dialogKeyName(DialogKey::Primary), "primary");
}
