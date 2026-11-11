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

#include <memory>

namespace Common {

// Frees a C library's object with the function its type is freed with
template <auto Free>
struct alignas(1) c_free_t final {
    template <typename T>
    void operator()(T * owned) const
    {
        Free(owned);
    }
};

// The owner of a C library's object: c_ptr_t<GVariant, g_variant_unref>, c_ptr_t<wl_registry, wl_registry_destroy>
template <typename T, auto Free>
using c_ptr_t = std::unique_ptr<T, c_free_t<Free>>;

} // namespace Common
