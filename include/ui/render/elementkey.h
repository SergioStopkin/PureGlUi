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

#include "common/hash.h"
#include "ui/render/uielement.h"
#include "ui/type.h"

#include <cstddef>
#include <functional>

namespace Ui::Render {

// One element drawn, as the main renderer's Animator keys it: its id is not enough, since
// a part shares the id of what it is part of - a tab's close glyph, its tab's
struct alignas(16) element_key_t final {
    id_t          id   = INVALID_ID;
    UiElementType type = UiElementType::MenuButton;

    bool operator==(const element_key_t &) const = default;
};

} // namespace Ui::Render

template <>
struct std::hash<Ui::Render::element_key_t> {
    std::size_t operator()(const Ui::Render::element_key_t & key) const noexcept
    {
        return Common::hashOf(key.id, key.type);
    }
};
