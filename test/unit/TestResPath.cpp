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
 * @file TestResPath.cpp
 * @brief Unit tests for Ui::Res::ResPath - the bundled-resource path resolver.
 *        Verifies every join method against a fixed base and confirms the output
 *        is forward-slash normalized (generic_string) so cache keys / logs stay
 *        platform-neutral.
 */

#include "ui/res/respath.h"

#include <gtest/gtest.h>
#include <string>

using Ui::Res::ResPath;

namespace {
ResPath resPath() { return ResPath("/base"); }
} // namespace

TEST(ResPath, IconAndFont)
{
    EXPECT_EQ(resPath().icon("close.svg"), "/base/icon/close.svg");
    EXPECT_EQ(resPath().fontDir(), "/base/font");
    EXPECT_EQ(resPath().fontFile("sans.ttf"), "/base/font/sans.ttf");
}

TEST(ResPath, ThemeComposesNameAndMode)
{
    EXPECT_EQ(resPath().theme("default", "dark"), "/base/submenu/theme/default-dark.json");
    EXPECT_EQ(resPath().theme("cobalt", "light"), "/base/submenu/theme/cobalt-light.json");
}

TEST(ResPath, TopLevelFiles)
{
    EXPECT_EQ(resPath().layoutFile(), "/base/layout.json");
    EXPECT_EQ(resPath().appFile(), "/base/app.json");
    EXPECT_EQ(resPath().dialogFile(), "/base/dialog.json");
    EXPECT_EQ(resPath().shortcutFile(), "/base/shortcut.json");
    EXPECT_EQ(resPath().iconDefaultsFile(), "/base/icon-defaults.json");
    EXPECT_EQ(resPath().inputFile(), "/base/input.json");
}

// The generic resolver a host uses for its own res, so no host filename needs a
// framework-side accessor.
TEST(ResPath, FileResolvesAnyName)
{
    EXPECT_EQ(resPath().file("render.json"), "/base/render.json");
    EXPECT_EQ(resPath().file("nested/thing.json"), "/base/nested/thing.json");
}

TEST(ResPath, SubmenuAndDirs)
{
    EXPECT_EQ(resPath().submenuRoot(), "/base/submenu");
    EXPECT_EQ(resPath().submenuDir("theme"), "/base/submenu/theme");
    EXPECT_EQ(resPath().buttonDir(), "/base/button");
    EXPECT_EQ(resPath().menuDir(), "/base/menu");
    EXPECT_EQ(resPath().dockDir(), "/base/dock");
}

TEST(ResPath, Locale) { EXPECT_EQ(resPath().locale("en"), "/base/locale/en.json"); }

TEST(ResPath, ForwardSlashNormalized)
{
    // No backslashes regardless of platform (generic_string()).
    const std::string layout = resPath().layoutFile();
    EXPECT_EQ(layout.find('\\'), std::string::npos);
}
