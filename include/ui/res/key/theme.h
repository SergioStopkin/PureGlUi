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

#include <string>

namespace Ui::Res::Key {

// The theme-file keys the ThemeStore looks up directly: the top-level display
// name plus the :root CSS custom-property variables (var(--X) map key) - the
// single source of truth for their spelling, resolved through themeKeyName().
// Block selectors live in ElementKey and property names in CssPropKey (both
// shared with layout); these variables are theme-only, so they live here.
// Spellings keep the CSS custom-property (--kebab) convention.
enum class ThemeKey : unsigned char {
    Name, // "name": theme display label (locale key), top-level field

    ClMain,        // --cl-main
    BgMain,        // --bg-main
    ClSecond,      // --cl-second
    BgSecond,      // --bg-second
    ClShadow,      // --cl-shadow
    ShadowOpacity, // --shadow-opacity
    ClModel,       // --cl-model
    ClError,       // --cl-error
    ClLoad,        // --cl-load
    ClInfo,        // --cl-info
    ClWarn,        // --cl-warn
    FontSans,      // --font-sans
    FontMono,      // --font-mono

    Count, // enumerator total, never a key - what an exhaustive check counts against
};

[[nodiscard]] inline std::string themeKeyName(ThemeKey key)
{
    switch (key) {
    case ThemeKey::Name: return "name";

    case ThemeKey::ClMain: return "--cl-main";
    case ThemeKey::BgMain: return "--bg-main";
    case ThemeKey::ClSecond: return "--cl-second";
    case ThemeKey::BgSecond: return "--bg-second";
    case ThemeKey::ClShadow: return "--cl-shadow";
    case ThemeKey::ShadowOpacity: return "--shadow-opacity";
    case ThemeKey::ClModel: return "--cl-model";
    case ThemeKey::ClError: return "--cl-error";
    case ThemeKey::ClLoad: return "--cl-load";
    case ThemeKey::ClInfo: return "--cl-info";
    case ThemeKey::ClWarn: return "--cl-warn";
    case ThemeKey::FontSans: return "--font-sans";
    case ThemeKey::FontMono: return "--font-mono";

    case ThemeKey::Count: break;
    }
    return {};
}

} // namespace Ui::Res::Key
