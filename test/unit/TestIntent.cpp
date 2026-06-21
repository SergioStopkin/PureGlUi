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
 * @file TestIntent.cpp
 * @brief Value-type guard for the intents-out vocabulary: Ui::IntentKind,
 *        Ui::intent_t (tagged record), Ui::result_t. Verifies defaults, the
 *        tagged-field convention, and equality. Pure C++ - no rendering.
 */

#include "ui/intent.h"
#include "ui/intentkind.h"
#include "ui/result.h"

#include <gtest/gtest.h>

namespace {

Ui::intent_t emitAction(Ui::key_t key, std::string arg = "")
{
    return Ui::intent_t { Ui::IntentKind::EmitAction, Ui::INVALID_ID, std::move(key), std::move(arg) };
}

} // namespace

TEST(Intent, Defaults)
{
    Ui::intent_t in;
    EXPECT_EQ(in.kind, Ui::IntentKind::EmitAction);
    EXPECT_EQ(in.id, Ui::INVALID_ID);
    EXPECT_TRUE(in.actionKey.empty());
    EXPECT_TRUE(in.arg.empty());
}

TEST(Intent, EmitActionCarriesKeyAndArg)
{
    const Ui::intent_t in = emitAction("SetDisplayMode", "shaded");
    EXPECT_EQ(in.kind, Ui::IntentKind::EmitAction);
    EXPECT_EQ(in.actionKey, "SetDisplayMode");
    EXPECT_EQ(in.arg, "shaded");
}

TEST(Intent, IdOnlyKinds)
{
    Ui::intent_t in;
    in.kind = Ui::IntentKind::SwitchTab;
    in.id   = 7;
    EXPECT_EQ(in.kind, Ui::IntentKind::SwitchTab);
    EXPECT_EQ(in.id, 7U);
}

TEST(Intent, Equality)
{
    const Ui::intent_t a = emitAction("Undo");
    Ui::intent_t       b = emitAction("Undo");
    EXPECT_EQ(a, b);
    b.arg = "x";
    EXPECT_NE(a, b);
}

TEST(Result, DefaultIsCleanAndEmpty)
{
    Ui::result_t r;
    EXPECT_FALSE(r.isDirty);
    EXPECT_TRUE(r.intents.empty());
}

TEST(Result, CarriesIntentsInOrder)
{
    Ui::result_t r;
    r.isDirty = true;
    r.intents.push_back(emitAction("A"));
    r.intents.push_back(Ui::intent_t { Ui::IntentKind::CloseTab, 3, {}, {} });

    ASSERT_EQ(r.intents.size(), 2U);
    EXPECT_EQ(r.intents[0].kind, Ui::IntentKind::EmitAction);
    EXPECT_EQ(r.intents[0].actionKey, "A");
    EXPECT_EQ(r.intents[1].kind, Ui::IntentKind::CloseTab);
    EXPECT_EQ(r.intents[1].id, 3U);
}
