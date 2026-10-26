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

#include "ui/render/uielementstate.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/dialogaction.h"

#include <string>

namespace Ui::Render::Popup {

// One button DialogRenderer lays out and draws in its bottom row
struct alignas(64) dialog_button_t final {
    Ui::Res::Type::bound_t      bound {};
    std::string                 label;
    Ui::Render::UiElementState  state   = Ui::Render::UiElementState::None;
    bool                        primary = false;
    Ui::Res::Type::DialogAction action  = Ui::Res::Type::DialogAction::None;

    bool operator==(const dialog_button_t &) const = default;
};

} // namespace Ui::Render::Popup
