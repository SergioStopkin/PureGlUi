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

/**
 * @brief Include X11 headers and undefine macros that clash with C++ code
 *
 * X11 headers define several preprocessor macros (None, Bool, Status,
 * KeyPress, KeyRelease) that collide with enum values, gtest internals,
 * and standard C++ identifiers. This header centralizes the includes
 * and undefs so every consumer does not have to repeat them.
 *
 * Include this header instead of <X11/Xlib.h> directly when the
 * translation unit also uses gtest, our EventType/KeyModifier enums,
 * or any other code that would clash with these macros.
 */

#ifdef __linux__

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

// X11 macro    Defined as    Clashes with
// ---------    ----------    ------------
// None         0L            EventType::None, KeyModifier::None, gtest internal::None
// Bool         int           gtest Bool() function
// Status       int           gtest Status usage
// True         1             C++ bool literal
// False        0             C++ bool literal
// KeyPress     2             EventType::KeyPress
// KeyRelease   3             EventType::KeyRelease

#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif

// X11 macro values preserved as typed C++ constants (defined after undefs)
namespace Ui::Backend::Window::Platform::X11 {
static constexpr int64_t None       = 0L;
static constexpr int     False      = 0;
static constexpr int     True       = 1;
static constexpr int     KeyPress   = 2;
static constexpr int     KeyRelease = 3;
} // namespace Ui::Backend::Window::Platform::X11

#endif // __linux__
