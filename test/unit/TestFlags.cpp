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
 * @file TestFlags.cpp
 * @brief Unit tests for Common::flags_t - a set of an enum's flags, combined and
 *        tested without casting a combination back to the enum
 */

#include "ui/render/eventkind.h"
#include "ui/res/type/changed.h"
#include "ui/window/keymodifier.h"

#include <gtest/gtest.h>

using Ui::Res::Type::Changed;
using Ui::Res::Type::changes_t;

TEST(Flags, EmptyHoldsNothing)
{
    const changes_t none;
    EXPECT_FALSE(none.has(Changed::Layout));
    EXPECT_EQ(none, Changed::None);
    EXPECT_EQ(none, changes_t());
}

// How LayoutStore reports the two parts one parse can change
TEST(Flags, BuiltFromSeveralHoldsEach)
{
    const changes_t both(Changed::Layout, Changed::Popup);
    EXPECT_TRUE(both.has(Changed::Layout));
    EXPECT_TRUE(both.has(Changed::Popup));
    EXPECT_FALSE(both.has(Changed::Theme));
    EXPECT_NE(both, Changed::Layout); // a set of two is neither of its flags alone
}

// None among several adds nothing: a store that changed nothing reports None
TEST(Flags, NoneAddsNothing) { EXPECT_EQ(changes_t(Changed::Theme, Changed::None), Changed::Theme); }

// How ResManager gathers what each store reports
TEST(Flags, OrAssignGathersFlagsAndSets)
{
    changes_t changed;
    changed |= Changed::Locale;
    changed |= changes_t(Changed::Layout, Changed::Popup);
    EXPECT_EQ(changed, changes_t(Changed::Locale, Changed::Layout, Changed::Popup));
}

TEST(Flags, HasAnyMeetsOnOneSharedFlag)
{
    const changes_t theme(Changed::Theme);
    EXPECT_TRUE(theme.hasAny(changes_t(Changed::Theme, Changed::Render)));
    EXPECT_FALSE(theme.hasAny(changes_t(Changed::Layout, Changed::Render)));
    EXPECT_FALSE(theme.hasAny(changes_t()));
}

// A set the size of its enum, so an event struct holding one pads no more than before
TEST(Flags, SizedAsItsEnum)
{
    EXPECT_EQ(sizeof(Ui::Window::key_modifiers_t), sizeof(Ui::Window::KeyModifier));
    EXPECT_EQ(sizeof(changes_t), sizeof(Changed));
}

// The accept masks are built at compile time
TEST(Flags, MasksAreConstant)
{
    static_assert(Ui::Render::ANY_DRAG.has(Ui::Render::EventKind::Drag));
    static_assert(!Ui::Render::ANY_DRAG.has(Ui::Render::EventKind::LeftClick));
    EXPECT_TRUE(Ui::Render::ANY_CLICK.has(Ui::Render::EventKind::DoubleClick));
}
