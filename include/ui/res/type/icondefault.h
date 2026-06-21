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

#include "ui/res/type/iconplace.h"

#include <string>

namespace Ui::Res::Type {

// One resolved entry from res/icon-defaults.json. After alias resolution `icon`
// is always a concrete .svg filename and `place` is materialized (default Left).
struct alignas(32) icon_default_t final {
    std::string              icon;                                   // resolved .svg filename
    Ui::Res::Type::IconPlace place = Ui::Res::Type::IconPlace::Left; // resolved placement

    bool operator==(const icon_default_t &) const = default;
};

} // namespace Ui::Res::Type
