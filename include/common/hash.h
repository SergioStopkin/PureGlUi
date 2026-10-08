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

#include "common/bit.h"

#include <cstddef>
#include <functional>

namespace Common {

// One hash of every value, folded in order the way Boost's hash_combine does it, so
// (a, b) and (b, a) hash apart - what a key of several fields hashes as
//
// std::size_t, what std::hash returns: a bucket number two keys may share, never an id
template <typename First, typename... Rest>
[[nodiscard]] std::size_t hashOf(const First & first, const Rest &... rest)
{
    // 2^32 over the golden ratio: odd, its bits in no pattern, so values close together hash far apart
    constexpr std::size_t GOLDEN_RATIO_32 = 0x9e3779b9;
    std::size_t           hash            = std::hash<First> {}(first);
    ((hash = Bit::Xor(hash, std::hash<Rest> {}(rest) + GOLDEN_RATIO_32 + Bit::Shl(hash, 6U) + Bit::Shr(hash, 2U))),
     ...);
    return hash;
}

} // namespace Common
