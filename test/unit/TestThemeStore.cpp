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
 *        The "host" block, against a fixture (the shipped themes carry none): each
 *        registered key handed its own value, var() resolved, on every load, in the
 *        order registered, a key registered twice keeping its first; a host edit
 *        alone, or a key first handed over, counting into the reload diff.
 */

#include "ui/color.h"
#include "ui/res/localemanager.h"
#include "ui/res/respath.h"
#include "ui/res/store/themestore.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/theme.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

using Ui::Color;
using Ui::Res::LocaleManager;
using Ui::Res::ResPath;
using Ui::Res::Store::ThemeStore;
using Ui::Res::Type::Changed;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }

// A theme whose "host" block writes edge, tint (a var()) and blank (empty)
std::string hostTheme() { return std::string(TEST_DATA_DIR) + "/themehost.json"; }

// The same theme with edge edited, and nothing else
std::string hostThemeEdited() { return std::string(TEST_DATA_DIR) + "/themehostedited.json"; }
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

// Every shipped theme, both modes, names each colour the tooltip and the toolbar
// buttons draw with: one it leaves out or misspells falls back to its error colour
TEST(ThemeStore, EveryThemeHasTooltipAndToolbarButtonColours)
{
    ThemeStore                     store;
    LocaleManager                  locale;
    const std::vector<std::string> names = store.scanThemeNames(resPath(), locale);
    ASSERT_FALSE(names.empty()); // or the loop checks nothing and passes
    for (const std::string & name : names) {
        for (const std::string mode : { "dark", "light" }) {
            SCOPED_TRACE(name + "-" + mode);
            store.setThemeModeValue(mode);
            (void)store.setThemeName(name, resPath());
            const Ui::Res::Type::theme_t & theme = store.theme();
            EXPECT_NE(theme.tooltip.fg, theme.colorError);
            EXPECT_NE(theme.tooltip.bg, theme.colorError);
            EXPECT_NE(theme.toolbarButton.bg, theme.colorError);
            EXPECT_NE(theme.toolbarButtonHover.bg, theme.colorError);
            EXPECT_NE(theme.toolbarButtonActive.bg, theme.colorError);
            EXPECT_NE(theme.toolbarButtonDisabledColor, theme.colorError);
        }
    }
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

// ============================================================================
// The "host" block - each value handed to what the host registered for its key
// ============================================================================

// Each key is handed its own value, a var() resolved, in the order registered rather
// than the order the file writes them in
TEST(ThemeStore, HostKeysAreHandedTheirValuesInTheOrderRegistered)
{
    ThemeStore                                       store;
    std::vector<std::pair<std::string, std::string>> handed;
    for (const std::string key : { "tint", "edge" }) {
        store.registerThemeParam(key, [&handed, key](const std::string & value) { handed.emplace_back(key, value); });
    }

    (void)store.loadTheme(hostTheme());
    EXPECT_EQ(handed, (std::vector<std::pair<std::string, std::string>> { { "tint", "#123456" }, { "edge", "20" } }));
}

// Every load hands every value over again, changed or not: the host's settings are set from
// the file loaded, never left to an earlier one
TEST(ThemeStore, EveryLoadHandsEveryHostValueOver)
{
    ThemeStore store;
    int        calls = 0;
    store.registerThemeParam("edge", [&calls](const std::string &) { ++calls; });

    (void)store.loadTheme(hostTheme());
    (void)store.loadTheme(hostTheme());
    EXPECT_EQ(calls, 2);
}

// An edit to the host block alone is a change, though theme_t holds none of it - else the
// host's settings would change and nothing downstream be told to read them again
TEST(ThemeStore, AHostEditAloneIsAChange)
{
    ThemeStore  store;
    std::string edge;
    store.registerThemeParam("edge", [&edge](const std::string & value) { edge = value; });

    EXPECT_NE(store.loadTheme(hostTheme()), Changed::None);
    EXPECT_EQ(store.loadTheme(hostTheme()), Changed::None);
    EXPECT_NE(store.loadTheme(hostThemeEdited()), Changed::None);
    EXPECT_EQ(edge, "80");
}

// A key registered after a load is a change at the next, though the theme is the same - the
// host has a value to take up - and a reload after that is none
TEST(ThemeStore, AHostKeyFirstHandedOverIsAChange)
{
    ThemeStore store;
    (void)store.loadTheme(hostTheme());
    std::string edge;
    store.registerThemeParam("edge", [&edge](const std::string & value) { edge = value; });

    EXPECT_NE(store.loadTheme(hostTheme()), Changed::None);
    EXPECT_EQ(edge, "20");
    EXPECT_EQ(store.loadTheme(hostTheme()), Changed::None);
}

// Negative: a key registered twice keeps its first - the second is never handed a value
TEST(ThemeStore, AHostKeyRegisteredTwiceKeepsItsFirst)
{
    ThemeStore store;
    int        first  = 0;
    int        second = 0;
    store.registerThemeParam("edge", [&first](const std::string &) { ++first; });
    store.registerThemeParam("edge", [&second](const std::string &) { ++second; });

    (void)store.loadTheme(hostTheme());
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 0);
}

// Negative: a key the block does not write, or writes empty, is handed nothing and changes
// nothing
TEST(ThemeStore, AHostKeyMissingOrEmptyIsHandedNothing)
{
    ThemeStore store;
    int        calls = 0;
    store.registerThemeParam("missing", [&calls](const std::string &) { ++calls; });
    store.registerThemeParam("blank", [&calls](const std::string &) { ++calls; });

    (void)store.loadTheme(hostTheme());
    EXPECT_EQ(store.loadTheme(hostTheme()), Changed::None);
    EXPECT_EQ(calls, 0);
}

// Negative: a theme with no "host" block hands nothing to any key
TEST(ThemeStore, AThemeWithNoHostBlockHandsNothing)
{
    ThemeStore store;
    int        calls = 0;
    store.registerThemeParam("edge", [&calls](const std::string &) { ++calls; });

    (void)store.loadCurrent(resPath());
    EXPECT_EQ(calls, 0);
}

// Negative: with no key registered, a reload of a theme with a host block changes nothing
TEST(ThemeStore, NoHostKeyRegisteredReloadsAsNothingChanged)
{
    ThemeStore store;
    (void)store.loadTheme(hostTheme());
    EXPECT_EQ(store.loadTheme(hostTheme()), Changed::None);
}
