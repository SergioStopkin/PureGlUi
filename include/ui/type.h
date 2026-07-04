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

#include <cstddef>
#include <functional>
#include <string>

// UI framework primitive aliases. The fw owns its own primitives so that
// include/ui never depends on include/pureglui (the dependency arrow
// points only app -> ui).
namespace Ui {

using id_t           = std::size_t;
using key_t          = std::string; // hierarchical element identity, e.g. "view:displayMode:shaded"; "" = none
using fpx_t          = float;
using surface_id_t   = std::size_t;
using font_handle_t  = std::size_t;
using image_handle_t = std::size_t;

using task_fn_t      = std::function<void()>;                        // run an argless callback
using provider_fn_t  = std::function<std::string()>;                 // produce the current value
using action_fn_t    = std::function<void(const std::string & arg)>; // consume an opaque arg
using predicate_fn_t = std::function<bool(const std::string & arg)>; // test a string

inline constexpr id_t INVALID_ID = static_cast<id_t>(-1);

} // namespace Ui
