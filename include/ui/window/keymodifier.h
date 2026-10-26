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

#include "common/flags.h"

#include <cstdint>

namespace Ui::Window {

/**
 * @brief Keyboard modifier flags
 */
enum class KeyModifier : uint8_t { None = 0, Shift = 1U << 0U, Control = 1U << 1U, Alt = 1U << 2U, Meta = 1U << 3U };

// The modifiers held at once
using key_modifiers_t = Common::flags_t<KeyModifier>;

} // namespace Ui::Window
