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
 * @file TestLocaleManager.cpp
 * @brief Unit tests for Ui::Res::LocaleManager - string lookup by JSON key.
 *        Covers dynamic set/get, the empty-string fallback for an unknown key,
 *        and load() success (shipped en.json) / failure (missing locale).
 */

#include "ui/res/localemanager.h"
#include "ui/res/respath.h"

#include <gtest/gtest.h>
#include <string>

using Ui::Res::LocaleManager;
using Ui::Res::ResPath;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }
} // namespace

TEST(LocaleManager, UnknownKeyReturnsEmpty)
{
    const LocaleManager locale;
    EXPECT_TRUE(locale.get("no-such-key").empty());
}

TEST(LocaleManager, SetThenGet)
{
    LocaleManager locale;
    locale.set("greeting", "Hello");
    EXPECT_EQ(locale.get("greeting"), "Hello");
    // Overwrite is last-write-wins.
    locale.set("greeting", "Hi");
    EXPECT_EQ(locale.get("greeting"), "Hi");
}

TEST(LocaleManager, LoadShippedLocaleSucceeds)
{
    LocaleManager locale;
    EXPECT_TRUE(locale.load("en", resPath()));
}

TEST(LocaleManager, LoadMissingLocaleFails)
{
    LocaleManager locale;
    EXPECT_FALSE(locale.load("zz-nonexistent", resPath()));
}
