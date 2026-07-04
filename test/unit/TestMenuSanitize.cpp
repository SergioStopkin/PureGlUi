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
 * @file TestMenuSanitize.cpp
 * @brief Pins the ingest rule "every string loaded from res JSON passes
 *        Common::Sanitize": hostile menu JSON (terminal escape bytes, control
 *        characters, overlong strings) must come out of parseMenuItem with all
 *        control bytes stripped and lengths capped. Pure C++ - builds the
 *        hostile JSON in memory.
 */

#include "common/sanitize.h"
#include "menustorefixture.h"
#include "nlohmann/json.hpp"
#include "ui/res/type/menu.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>

namespace {

// True when the string carries no control bytes (newline and tab allowed -
// the same contract Common::Sanitize::string enforces).
bool isClean(const std::string & s)
{
    return std::all_of(s.begin(), s.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte >= 0x20 || c == '\n' || c == '\t';
    });
}

// Every string field parseMenuItem ingests, poisoned with ESC/control bytes.
nlohmann::json hostileItemJson()
{
    return { { "label",
               "Fi\x1B"
               "[31mle\x01" },
             { "action",
               "Open\x1B"
               "File" },
             { "shortcut",
               "Ctrl+\x07"
               "O" },
             { "submenus",
               { { "auto",
                   "th\x1B"
                   "emes" },
                 { "action",
                   "Sw\x02"
                   "itch" } } },
             { "dialog",
               { { "type", "Info" },
                 { "title",
                   "Ab\x1B"
                   "out" },
                 { "content",
                   "Bo\x0C"
                   "dy" },
                 { "link",
                   "li\x1B"
                   "nk" } } } };
}

TEST(MenuSanitize, ControlBytesStrippedFromAllIngestedStrings)
{
    TestSupport::menu_store_fixture_t fx;
    const Ui::Res::Type::menu_t       item = fx.store.parseMenuItem(hostileItemJson(), 1);

    EXPECT_TRUE(isClean(item.label)) << item.label;
    EXPECT_TRUE(isClean(item.actionKey)) << item.actionKey;
    EXPECT_TRUE(isClean(item.shortcut)) << item.shortcut;
    EXPECT_TRUE(isClean(item.submenu)) << item.submenu;
    EXPECT_TRUE(isClean(item.submenuActionKey)) << item.submenuActionKey;
    EXPECT_TRUE(isClean(item.dialog.title)) << item.dialog.title;
    EXPECT_TRUE(isClean(item.dialog.content)) << item.dialog.content;
    EXPECT_TRUE(isClean(item.dialog.link)) << item.dialog.link;
}

// Sanitization strips the hostile bytes but keeps the legitimate text.
TEST(MenuSanitize, LegitimateTextSurvives)
{
    TestSupport::menu_store_fixture_t fx;
    const Ui::Res::Type::menu_t       item = fx.store.parseMenuItem(hostileItemJson(), 1);

    EXPECT_EQ(item.label, "Fi[31mle");
    EXPECT_EQ(item.actionKey, "OpenFile");
    EXPECT_EQ(item.shortcut, "Ctrl+O");
    EXPECT_EQ(item.dialog.title, "About");
}

// Overlong strings are capped at the Sanitize limit.
TEST(MenuSanitize, OverlongLabelIsCapped)
{
    TestSupport::menu_store_fixture_t fx;
    nlohmann::json                    j    = { { "label", std::string(20000, 'A') } };
    const Ui::Res::Type::menu_t       item = fx.store.parseMenuItem(j, 1);
    EXPECT_LE(item.label.size(), Common::Sanitize::MAX_STRING_LENGTH);
}

} // namespace
