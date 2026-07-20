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
 * @file TestThemeStore.cpp
 * @brief Unit tests for Ui::Res::Store::ThemeStore. Loads the shipped themes
 *        (TEST_RES_DIR), verifies name/mode state + the derived theme icon, the
 *        mode toggle and name switch, the missing-file no-op, and scanThemeNames
 *        (discovery, "default"-first ordering, per-theme preview color cache).
 */

#include "ui/color.h"
#include "ui/res/localemanager.h"
#include "ui/res/respath.h"
#include "ui/res/store/themestore.h"
#include "ui/res/type/changed.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using Ui::Color;
using Ui::Res::LocaleManager;
using Ui::Res::ResPath;
using Ui::Res::Store::ThemeStore;
using Ui::Res::Type::Changed;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }
} // namespace

TEST(ThemeStore, DefaultsBeforeLoad)
{
    const ThemeStore store;
    EXPECT_EQ(store.themeName(), "default");
    EXPECT_EQ(store.themeMode(), "dark");
    EXPECT_TRUE(store.isThemeDark());
    EXPECT_EQ(store.themeIcon(), "theme-sun.svg");
}

TEST(ThemeStore, LoadCurrentChangesThenReloadNoOp)
{
    ThemeStore store;
    EXPECT_NE(store.loadCurrent(resPath()), Changed::None);
    // Re-loading the same file with identical values changes nothing.
    EXPECT_EQ(store.loadCurrent(resPath()), Changed::None);
}

TEST(ThemeStore, SwitchThemeModeTogglesAndUpdatesIcon)
{
    ThemeStore store;
    (void)store.loadCurrent(resPath());
    ASSERT_TRUE(store.isThemeDark());

    (void)store.switchThemeMode(resPath());
    EXPECT_EQ(store.themeMode(), "light");
    EXPECT_FALSE(store.isThemeDark());
    EXPECT_EQ(store.themeIcon(), "theme-moon.svg");

    (void)store.switchThemeMode(resPath());
    EXPECT_EQ(store.themeMode(), "dark");
    EXPECT_TRUE(store.isThemeDark());
}

TEST(ThemeStore, SetThemeNameReloads)
{
    ThemeStore store;
    (void)store.loadCurrent(resPath());
    (void)store.setThemeName("cobalt", resPath());
    EXPECT_EQ(store.themeName(), "cobalt");
}

TEST(ThemeStore, MissingFileIsNoOp)
{
    ThemeStore store;
    EXPECT_EQ(store.loadTheme(std::string(TEST_RES_DIR) + "/submenu/theme/no-such-theme.json"), Changed::None);
}

TEST(ThemeStore, RawSettersDoNotReload)
{
    ThemeStore store;
    store.setThemeNameValue("graphite");
    store.setThemeModeValue("light");
    EXPECT_EQ(store.themeName(), "graphite");
    EXPECT_EQ(store.themeMode(), "light");
    EXPECT_FALSE(store.isThemeDark());
}

TEST(ThemeStore, ScanThemeNamesDiscoversAndOrders)
{
    ThemeStore                     store;
    LocaleManager                  locale;
    const std::vector<std::string> names = store.scanThemeNames(resPath(), locale);

    ASSERT_FALSE(names.empty());
    EXPECT_EQ(names.front(), "default"); // "default" always first
    EXPECT_GE(names.size(), 4U);         // default, cobalt, emerald, graphite (at least)
    EXPECT_NE(std::find(names.begin(), names.end(), "cobalt"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "graphite"), names.end());
    // The rest are sorted alphabetically after "default".
    EXPECT_TRUE(std::is_sorted(names.begin() + 1, names.end()));
}

TEST(ThemeStore, PreviewColorsCachedForKnownMissingForUnknown)
{
    ThemeStore    store;
    LocaleManager locale;
    (void)store.scanThemeNames(resPath(), locale);

    // Known theme: its dark background is a real color, not the magenta error default.
    const Color magenta { 255, 0, 255, 255 };
    const auto  known = store.themePreviewColors("default");
    EXPECT_NE(known.first.bg, magenta);

    // Unknown theme: zero-initialized pair (fg falls back to the Color default).
    const auto missing = store.themePreviewColors("no-such-theme");
    EXPECT_EQ(missing.first.fg, magenta);
}
