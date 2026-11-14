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

// The keywords a `transition` value is spelled with in res, CSS's - the single source of
// truth for their spelling, resolved through transitionKeyName()
enum class TransitionKey : unsigned char {
    All,         // every property the engine animates
    Linear,      // cubic-bezier(0, 0, 1, 1)
    Ease,        // cubic-bezier(0.25, 0.1, 0.25, 1), the default
    EaseIn,      // cubic-bezier(0.42, 0, 1, 1)
    EaseOut,     // cubic-bezier(0, 0, 0.58, 1)
    EaseInOut,   // cubic-bezier(0.42, 0, 0.58, 1)
    CubicBezier, // cubic-bezier(x1, y1, x2, y2)
};

[[nodiscard]] inline std::string transitionKeyName(TransitionKey key)
{
    switch (key) {
    case TransitionKey::All: return "all";
    case TransitionKey::Linear: return "linear";
    case TransitionKey::Ease: return "ease";
    case TransitionKey::EaseIn: return "ease-in";
    case TransitionKey::EaseOut: return "ease-out";
    case TransitionKey::EaseInOut: return "ease-in-out";
    case TransitionKey::CubicBezier: return "cubic-bezier";
    }
    return {};
}

} // namespace Ui::Res::Key
