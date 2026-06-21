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

#include <type_traits>

namespace Common {

class Bit final {
public:
    Bit()  = delete;
    ~Bit() = delete;

    Bit(const Bit &)             = delete;
    Bit & operator=(const Bit &) = delete;

    template <typename T1, typename T2>
    static constexpr auto Or(T1 a, T2 b)
    {
        using U1     = std::make_unsigned_t<T1>;
        using U2     = std::make_unsigned_t<T2>;
        using Common = std::common_type_t<U1, U2>;
        return static_cast<Common>(static_cast<U1>(a)) | static_cast<Common>(static_cast<U2>(b));
    }

    template <typename T1, typename T2>
    static constexpr auto And(T1 a, T2 b)
    {
        using U1     = std::make_unsigned_t<T1>;
        using U2     = std::make_unsigned_t<T2>;
        using Common = std::common_type_t<U1, U2>;
        return static_cast<Common>(static_cast<U1>(a)) & static_cast<Common>(static_cast<U2>(b));
    }

    template <typename T1, typename T2>
    static constexpr auto Xor(T1 a, T2 b)
    {
        using U1     = std::make_unsigned_t<T1>;
        using U2     = std::make_unsigned_t<T2>;
        using Common = std::common_type_t<U1, U2>;
        return static_cast<Common>(static_cast<U1>(a)) ^ static_cast<Common>(static_cast<U2>(b));
    }

    template <typename T>
    static constexpr auto Shl(T val, unsigned int shift)
    {
        using U = std::make_unsigned_t<T>;
        return static_cast<U>(val) << shift;
    }

    template <typename T>
    static constexpr auto Shr(T val, unsigned int shift)
    {
        using U = std::make_unsigned_t<T>;
        return static_cast<U>(val) >> shift;
    }
};

} // namespace Common
