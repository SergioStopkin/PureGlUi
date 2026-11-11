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

// The message bus's own names: it says when a name changes owner. C strings, since GDBus takes
// nothing else
namespace Ui::Io::Platform::DBus {

inline constexpr const char * NAME               = "org.freedesktop.DBus"; // the bus's, and its interface's
inline constexpr const char * OBJECT_PATH        = "/org/freedesktop/DBus";
inline constexpr const char * NAME_OWNER_CHANGED = "NameOwnerChanged";

} // namespace Ui::Io::Platform::DBus
