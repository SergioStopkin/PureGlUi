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

#include "ui/res/type/dialogtype.h"
#include "ui/type.h"

#include <string>

namespace Ui::Res::Type {

// Dialog definition - all text comes from locale
struct alignas(128) dialog_t final {
    DialogType   type = DialogType::Info;
    std::string  title;           // locale key for dialog title
    std::string  content;         // locale key for dialog body text
    std::string  link;            // locale key for URL (InfoLink dialogs)
    std::string  icon;            // SVG icon filename (e.g. "2139.svg")
    std::string  file;            // file path for content (overrides locale content)
    std::wstring contentOverride; // runtime content (resolved at open time)
    fpx_t        width {};        // per-dialog override (0 = use layout default)
    fpx_t        height {};

    bool operator==(const dialog_t &) const = default;
};

} // namespace Ui::Res::Type
