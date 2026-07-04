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

#pragma once

namespace Common {

// Mixin for non-value types - those that hold a reference/const member or own a
// resource. Inherit privately to delete copy and move, stating the type's
// non-copyable, non-movable nature in one place instead of four = delete lines
// per class.
//
// Protected non-virtual dtor (Core Guideline C.35): used only as a private base,
// never deleted polymorphically, so no vtable - empty-base optimization keeps it
// zero-size.
class NonCopyable {
protected:
    NonCopyable()  = default;
    ~NonCopyable() = default;

public:
    NonCopyable(const NonCopyable &)             = delete;
    NonCopyable(NonCopyable &&)                  = delete;
    NonCopyable & operator=(const NonCopyable &) = delete;
    NonCopyable & operator=(NonCopyable &&)      = delete;
};

} // namespace Common
