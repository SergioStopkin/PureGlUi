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

#include "ui/type.h"

#include <optional>
#include <string>

namespace Ui::Res::Type {

// One key of a theme file's "host" block, as ThemeStore keeps it. The value is opaque to
// the framework, but comparing it is what lets a host-only edit report Changed
struct alignas(128) host_param_t final {
    Ui::action_fn_t            apply; // what the host does with the value
    std::optional<std::string> value; // the one last handed to apply; none until one is
};

} // namespace Ui::Res::Type
