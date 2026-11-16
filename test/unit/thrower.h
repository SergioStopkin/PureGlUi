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

#include <stdexcept>

namespace TestSupport {

// A value whose move throws when it says so: a container's node failing as it is built
struct alignas(8) thrower_t final {
    bool isThrowing = false;

    thrower_t() = default;
    explicit thrower_t(bool throws)
        : isThrowing(throws)
    {
    }
    thrower_t(thrower_t && other)
        : isThrowing(other.isThrowing)
    {
        if (isThrowing) {
            throw std::runtime_error("a node failing as it is built");
        }
    }
    thrower_t(const thrower_t &)             = default;
    thrower_t & operator=(const thrower_t &) = default;
    thrower_t & operator=(thrower_t &&)      = default;
    ~thrower_t()                             = default;
};

} // namespace TestSupport
