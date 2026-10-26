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

#include <concepts>
#include <type_traits>

namespace Common {

// A set of an enum's single-bit flags. The enum names each bit; a set holds their bits
// and is never cast back to the enum, where a combination names no enumerator
template <typename E>
    requires std::is_enum_v<E>
struct alignas(sizeof(E)) flags_t final {
    using bits_t = std::underlying_type_t<E>;

    constexpr flags_t() = default;

    constexpr explicit flags_t(E flag, std::same_as<E> auto... more)
        : m_bits(static_cast<bits_t>((bitsOf(flag) | ... | bitsOf(more))))
    {
    }

    constexpr flags_t & operator|=(E flag)
    {
        m_bits = static_cast<bits_t>(m_bits | bitsOf(flag));
        return *this;
    }

    constexpr flags_t & operator|=(flags_t other)
    {
        m_bits = static_cast<bits_t>(m_bits | other.m_bits);
        return *this;
    }

    [[nodiscard]] constexpr bool has(E flag) const { return (m_bits & bitsOf(flag)) != 0; }

    [[nodiscard]] constexpr bool hasAny(flags_t other) const { return (m_bits & other.m_bits) != 0; }

    bool operator==(const flags_t &) const = default;

    // The set is exactly this one flag, None the empty set
    constexpr bool operator==(E flag) const { return m_bits == bitsOf(flag); }

private:
    static constexpr bits_t bitsOf(E flag) { return static_cast<bits_t>(flag); }

    bits_t m_bits {};
};

} // namespace Common
