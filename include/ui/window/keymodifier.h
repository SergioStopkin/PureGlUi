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

#include <cstdint>

namespace Ui::Window {

/**
 * @brief Keyboard modifier flags
 */
enum class KeyModifier : uint32_t { None = 0, Shift = 1U << 0U, Control = 1U << 1U, Alt = 1U << 2U, Meta = 1U << 3U };

inline uint32_t toUint(KeyModifier m) { return static_cast<uint32_t>(m); }

inline KeyModifier toKeyModifier(uint32_t v) { return static_cast<KeyModifier>(v); }

inline KeyModifier operator|(KeyModifier a, KeyModifier b) { return toKeyModifier(toUint(a) | toUint(b)); }

inline KeyModifier operator&(KeyModifier a, KeyModifier b) { return toKeyModifier(toUint(a) & toUint(b)); }

inline bool hasModifier(KeyModifier modifiers, KeyModifier flag) { return (toUint(modifiers) & toUint(flag)) != 0; }

} // namespace Ui::Window
