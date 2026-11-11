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

// xdg-desktop-portal's names on the session bus, for its FileChooser and the Request it answers on.
// C strings, since GDBus takes nothing else
namespace Ui::Io::Platform::Portal {

inline constexpr const char * BUS_NAME    = "org.freedesktop.portal.Desktop";
inline constexpr const char * OBJECT_PATH = "/org/freedesktop/portal/desktop";
inline constexpr const char * REQUEST_PATH =
"/org/freedesktop/portal/desktop/request/"; // then the caller's name and token

inline constexpr const char * X11_PREFIX     = "x11:";     // a parent window's identifier: then its XID in hex
inline constexpr const char * WAYLAND_PREFIX = "wayland:"; // or then the handle xdg-foreign exported it as

inline constexpr const char * FILE_CHOOSER = "org.freedesktop.portal.FileChooser";
inline constexpr const char * OPEN_FILE    = "OpenFile";
inline constexpr const char * REQUEST      = "org.freedesktop.portal.Request";
inline constexpr const char * RESPONSE     = "Response";
inline constexpr const char * CLOSE        = "Close";

inline constexpr const char * HANDLE_TOKEN   = "handle_token"; // OpenFile's options
inline constexpr const char * MULTIPLE       = "multiple";
inline constexpr const char * MODAL          = "modal";
inline constexpr const char * FILTERS        = "filters";
inline constexpr const char * CURRENT_FOLDER = "current_folder";
inline constexpr const char * URIS           = "uris"; // Response's results

} // namespace Ui::Io::Platform::Portal
