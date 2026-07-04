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
 * @file TestNonCopyable.cpp
 * @brief Unit tests for Common::NonCopyable: it deletes copy and move for itself
 *        and every type that inherits it, stays default-constructible through a
 *        derived type, and adds zero size (empty-base optimization).
 */

#include "common/noncopyable.h"

#include <gtest/gtest.h>
#include <memory>
#include <type_traits>

namespace {

using Common::NonCopyable;

// Intended usage: a resource-ish type inherits NonCopyable privately.
struct Widget final : private NonCopyable {
    int value = 7;
};

// NonCopyable itself: copy and move are deleted.
TEST(NonCopyable, BaseDeletesCopyAndMove)
{
    static_assert(!std::is_copy_constructible_v<NonCopyable>, "copy ctor must be deleted");
    static_assert(!std::is_copy_assignable_v<NonCopyable>, "copy assign must be deleted");
    static_assert(!std::is_move_constructible_v<NonCopyable>, "move ctor must be deleted");
    static_assert(!std::is_move_assignable_v<NonCopyable>, "move assign must be deleted");

    EXPECT_FALSE(std::is_copy_constructible_v<NonCopyable>);
    EXPECT_FALSE(std::is_copy_assignable_v<NonCopyable>);
    EXPECT_FALSE(std::is_move_constructible_v<NonCopyable>);
    EXPECT_FALSE(std::is_move_assignable_v<NonCopyable>);
}

// The protected ctor/dtor mean NonCopyable cannot be used as a standalone value -
// only as a base. So it is not default-constructible from the outside.
TEST(NonCopyable, BaseIsNotStandaloneConstructible)
{
    static_assert(!std::is_default_constructible_v<NonCopyable>, "protected ctor: not usable standalone");
    EXPECT_FALSE(std::is_default_constructible_v<NonCopyable>);
}

// A deriving type inherits the deleted copy/move: it becomes non-copyable,
// non-movable - the whole point of the mixin.
TEST(NonCopyable, DerivedInheritsNonCopyableNonMovable)
{
    static_assert(!std::is_copy_constructible_v<Widget>, "derived copy ctor must be deleted");
    static_assert(!std::is_copy_assignable_v<Widget>, "derived copy assign must be deleted");
    static_assert(!std::is_move_constructible_v<Widget>, "derived move ctor must be deleted");
    static_assert(!std::is_move_assignable_v<Widget>, "derived move assign must be deleted");

    EXPECT_FALSE(std::is_copy_constructible_v<Widget>);
    EXPECT_FALSE(std::is_copy_assignable_v<Widget>);
    EXPECT_FALSE(std::is_move_constructible_v<Widget>);
    EXPECT_FALSE(std::is_move_assignable_v<Widget>);
}

// Deriving keeps the type default-constructible and otherwise usable.
TEST(NonCopyable, DerivedStaysDefaultConstructible)
{
    static_assert(std::is_default_constructible_v<Widget>, "derived must stay default-constructible");

    Widget widget;
    EXPECT_EQ(widget.value, 7);
    widget.value = 42;
    EXPECT_EQ(widget.value, 42);
}

// A non-copyable type is still heap-allocatable and ownable by unique_ptr.
TEST(NonCopyable, DerivedWorksWithUniquePtr)
{
    auto widget = std::make_unique<Widget>();
    ASSERT_NE(widget, nullptr);
    widget->value = 99;
    EXPECT_EQ(widget->value, 99);
}

// Empty-base optimization: the mixin adds no size to the deriving type.
TEST(NonCopyable, AddsNoSize)
{
    static_assert(sizeof(Widget) == sizeof(int), "NonCopyable must be zero-size (empty-base optimization)");
    EXPECT_EQ(sizeof(Widget), sizeof(int));
}

} // namespace
