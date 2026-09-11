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

#include "common/unicode.h"
#include "ui/codepoint.h"
#include "ui/interface/irender.h"
#include "ui/type.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace Ui::Render {

// Fitting text to a box. Nothing here holds state or knows what is being drawn:
// the caller supplies the measuring sink, so these belong to no layer and every
// one of them - chrome, docks, popups - reaches the same two.
//
// Both work on std::wstring internally so substr() walks by codepoint, not by
// byte. A UTF-8 std::string would slice multi-byte sequences in half mid-loop
// and pass invalid bytes to textWidth(). Conversion happens at the entry / exit
// boundary only.
//
// Both return the original text unchanged when measurement is unavailable (null
// sink / font handle 0), so a caller never silently loses data.

// The longest prefix of wtext that still leaves reservedW free inside maxW,
// empty when not even one codepoint does. Sliced as a view, so a long label
// costs no allocation per codepoint
[[nodiscard]] inline std::wstring
fittingPrefix(std::wstring_view wtext, fpx_t maxW, fpx_t reservedW, Ui::IRender & render, Ui::font_handle_t font)
{
    std::size_t fit = 0;
    while (fit < wtext.size() && render.textWidth(font, wtext.substr(0, fit + 1)) + reservedW <= maxW) {
        ++fit;
    }
    return std::wstring(wtext.substr(0, fit));
}

// Prefix + ellipsis, else the ellipsis alone, else nothing - all of plain
// truncation, and the tail of the file-name ladder.
//
// Single horizontal-three-dot glyph (~3-5 CSS) vs the three ASCII dots it
// replaces (~8-12 CSS); the difference is what makes the tight end land on
// something useful
[[nodiscard]] inline std::string
ellipsize(std::wstring_view wtext, fpx_t maxW, Ui::IRender & render, Ui::font_handle_t font)
{
    const std::wstring ellipsis = Ui::wstr(Ui::Codepoint::ThreeDotH);
    const fpx_t        ellipW   = render.textWidth(font, ellipsis);
    const std::wstring best     = fittingPrefix(wtext, maxW, ellipW, render, font);
    if (!best.empty()) {
        return Common::Unicode::toUtf8(best + ellipsis);
    }
    return (ellipW <= maxW) ? Common::Unicode::toUtf8(ellipsis) : std::string {};
}

// Plain end-truncation: prefix + ellipsis, no filename logic. What a caller
// wants for text that is not a file name - "0.085 mm2" would otherwise be split
// into a name and an "extension" at the decimal point.
[[nodiscard]] inline std::string
truncateText(const std::string & text, fpx_t maxW, Ui::IRender * render, Ui::font_handle_t font)
{
    if (render == nullptr || font == 0) {
        return text;
    }
    if (maxW <= 0) {
        return {};
    }

    const std::wstring wtext = Common::Unicode::fromUtf8(text);
    if (render->textWidth(font, wtext) <= maxW) {
        return text;
    }
    return ellipsize(wtext, maxW, *render, font);
}

// Fit a filename within maxW CSS pixels via a degradation ladder:
//   1. full text                  if it fits
//   2. prefix + ellipsis + ext    longest prefix that still leaves room for
//                                 ellipsis + extension (preferred - keeps the
//                                 file-type cue visible)
//   3. prefix + ellipsis          step 2 can't fit even one prefix char; drop
//                                 the ext and keep as much name as fits
//   4. ellipsis alone             nothing else fits
//   5. ""                         even the ellipsis is wider than maxW
[[nodiscard]] inline std::string
truncateFileName(const std::string & text, fpx_t maxW, Ui::IRender * render, Ui::font_handle_t font)
{
    if (render == nullptr || font == 0) {
        return text;
    }
    if (maxW <= 0) {
        return {};
    }

    const std::wstring wtext = Common::Unicode::fromUtf8(text);
    if (render->textWidth(font, wtext) <= maxW) {
        return text;
    }

    // Split name and extension. Leading dot (".hidden") stays in the name;
    // dot > 0 enforces that.
    std::wstring wbase = wtext;
    std::wstring wext;
    const auto   dot = wtext.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0) {
        wext  = wtext.substr(dot + 1);
        wbase = wtext.substr(0, dot);
    }

    // Step 2: prefix + ellipsis + ext.
    if (!wext.empty()) {
        const std::wstring ellipsis = Ui::wstr(Ui::Codepoint::ThreeDotH);
        const fpx_t        needed   = render->textWidth(font, ellipsis) + render->textWidth(font, wext);
        if (needed < maxW) {
            const std::wstring best = fittingPrefix(wbase, maxW, needed, *render, font);
            if (!best.empty()) {
                return Common::Unicode::toUtf8(best + ellipsis + wext);
            }
        }
    }

    // Steps 3-5: drop the extension, keep as much recognisable name as fits
    return ellipsize(wbase, maxW, *render, font);
}

} // namespace Ui::Render
