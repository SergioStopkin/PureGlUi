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

#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace Ui {

/**
 * @brief Unified color representation for all rendering systems
 *
 * Stores color as RGBA bytes (0-255) and provides conversion methods for:
 * - OpenGL (float arrays, 0.0-1.0)
 * - Hex strings (#RRGGBB, #RRGGBBAA)
 */
class Color final {
    // Default magenta for debug
    uint8_t m_r       = 255;
    uint8_t m_g       = 0;
    uint8_t m_b       = 255;
    uint8_t m_a       = 255;
    bool    m_inherit = false;

public:
    // Constructors
    Color() = default;

    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : m_r(r)
        , m_g(g)
        , m_b(b)
        , m_a(a)
    {
    }

    Color(int r, int g, int b, int a = 255)
        : m_r(static_cast<uint8_t>(r))
        , m_g(static_cast<uint8_t>(g))
        , m_b(static_cast<uint8_t>(b))
        , m_a(static_cast<uint8_t>(a))
    {
    }

    static Color Inherit()
    {
        Color c;
        c.m_inherit = true;
        return c;
    }

    [[nodiscard]] bool isInherit() const { return m_inherit; }

    static Color TransparentBlack() { return { 0, 0, 0, 0 }; }

    // Parse from hex string (#RRGGBB or #RRGGBBAA)
    static Color fromHex(const std::string & hex)
    {
        if (hex.empty() || hex[0] != '#') {
            return {}; // Return default (magenta)
        }

        const std::string hexDigits = hex.substr(1);

        // Support #RGB short form
        if (hexDigits.length() == 3) {
            const int r = std::stoi(hexDigits.substr(0, 1), nullptr, 16) * 17; // 0-F -> 0-255
            const int g = std::stoi(hexDigits.substr(1, 1), nullptr, 16) * 17;
            const int b = std::stoi(hexDigits.substr(2, 1), nullptr, 16) * 17;
            return { r, g, b };
        }

        // Support #RRGGBB
        if (hexDigits.length() == 6) {
            const int r = std::stoi(hexDigits.substr(0, 2), nullptr, 16);
            const int g = std::stoi(hexDigits.substr(2, 2), nullptr, 16);
            const int b = std::stoi(hexDigits.substr(4, 2), nullptr, 16);
            return { r, g, b };
        }

        // Support #RRGGBBAA
        if (hexDigits.length() == 8) {
            const int r = std::stoi(hexDigits.substr(0, 2), nullptr, 16);
            const int g = std::stoi(hexDigits.substr(2, 2), nullptr, 16);
            const int b = std::stoi(hexDigits.substr(4, 2), nullptr, 16);
            const int a = std::stoi(hexDigits.substr(6, 2), nullptr, 16);
            return { r, g, b, a };
        }

        return {}; // Invalid format, return default
    }

    // Getters
    [[nodiscard]] uint8_t r() const { return m_r; }
    [[nodiscard]] uint8_t g() const { return m_g; }
    [[nodiscard]] uint8_t b() const { return m_b; }
    [[nodiscard]] uint8_t a() const { return m_a; }

    // Comparison operators
    bool operator==(const Color & other) const
    {
        return m_r == other.m_r && m_g == other.m_g && m_b == other.m_b && m_a == other.m_a;
    }

    bool operator!=(const Color & other) const { return !(*this == other); }

    // Conversion to different formats

    // OpenGL float array [r, g, b] (0.0-1.0)
    [[nodiscard]] std::array<float, 3> toGLRGB() const { return { m_r / 255.0F, m_g / 255.0F, m_b / 255.0F }; }

    // OpenGL float array [r, g, b, a] (0.0-1.0)
    [[nodiscard]] std::array<float, 4> toGLRGBA() const
    {
        return { m_r / 255.0F, m_g / 255.0F, m_b / 255.0F, m_a / 255.0F };
    }

    // Pack into ARGB32 (non-premultiplied): 0xAARRGGBB
    [[nodiscard]] uint32_t toArgb32() const
    {
        return (static_cast<uint32_t>(m_a) << 24U) | (static_cast<uint32_t>(m_r) << 16U)
             | (static_cast<uint32_t>(m_g) << 8U) | (static_cast<uint32_t>(m_b));
    }

    // Convert to hex string (#RRGGBB or #RRGGBBAA)
    [[nodiscard]] std::string toHex(bool includeAlpha = false) const
    {
        std::ostringstream oss;
        oss << '#' << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(m_r) << std::setw(2)
            << static_cast<int>(m_g) << std::setw(2) << static_cast<int>(m_b);
        if (includeAlpha) {
            oss << std::setw(2) << static_cast<int>(m_a);
        }
        return oss.str();
    }

    // Edge color from face: lighten on dark themes, darken on light themes.
    // Magnitude is the per-theme HSL L delta in [0, 100].
    [[nodiscard]] Color edgeColor(float contrast, bool isDarkTheme) const
    {
        return isDarkTheme ? Lighter(contrast) : Darker(contrast);
    }

    // Lighter(p): increase HSL lightness by p percentage points (0-100)
    [[nodiscard]] Color Lighter(const float p) const
    {
        float h = 0;
        float s = 0;
        float l = 0;
        toHSL(h, s, l);
        l = std::fmin(l + p, 100.0F);
        return fromHSL(h, s, l, m_a);
    }

    // Darker(p): decrease HSL lightness by p percentage points (0-100)
    [[nodiscard]] Color Darker(const float p) const
    {
        float h = 0;
        float s = 0;
        float l = 0;
        toHSL(h, s, l);
        l = std::fmax(l - p, 0.0F);
        return fromHSL(h, s, l, m_a);
    }

private:
    // Convert RGB to HSL (h: 0-360, s: 0-100, l: 0-100)
    void toHSL(float & h, float & s, float & l) const
    {
        const float rf = m_r / 255.0F;
        const float gf = m_g / 255.0F;
        const float bf = m_b / 255.0F;

        const float cmax = std::fmax(rf, std::fmax(gf, bf));
        const float cmin = std::fmin(rf, std::fmin(gf, bf));
        const float d    = cmax - cmin;

        l = (cmax + cmin) / 2.0F * 100.0F;

        if (d < 1e-6F) {
            h = 0;
            s = 0;
            return;
        }

        s = d / (1.0F - std::fabs(2.0F * (cmax + cmin) / 2.0F - 1.0F)) * 100.0F;

        if (cmax == rf) {
            h = 60.0F * std::fmod((gf - bf) / d, 6.0F);
        } else if (cmax == gf) {
            h = 60.0F * ((bf - rf) / d + 2.0F);
        } else {
            h = 60.0F * ((rf - gf) / d + 4.0F);
        }

        if (h < 0) {
            h += 360.0F;
        }
    }

    // Convert HSL to Color (h: 0-360, s: 0-100, l: 0-100)
    static Color fromHSL(float h, float s, float l, uint8_t a)
    {
        s /= 100.0F;
        l /= 100.0F;

        const float c = (1.0F - std::fabs(2.0F * l - 1.0F)) * s;
        const float x = c * (1.0F - std::fabs(std::fmod(h / 60.0F, 2.0F) - 1.0F));
        const float m = l - c / 2.0F;

        float rf = 0;
        float gf = 0;
        float bf = 0;

        if (h < 60.0F) {
            rf = c;
            gf = x;
        } else if (h < 120.0F) {
            rf = x;
            gf = c;
        } else if (h < 180.0F) {
            gf = c;
            bf = x;
        } else if (h < 240.0F) {
            gf = x;
            bf = c;
        } else if (h < 300.0F) {
            rf = x;
            bf = c;
        } else {
            rf = c;
            bf = x;
        }

        return { static_cast<uint8_t>(std::round((rf + m) * 255.0F)),
                 static_cast<uint8_t>(std::round((gf + m) * 255.0F)),
                 static_cast<uint8_t>(std::round((bf + m) * 255.0F)),
                 a };
    }
};

} // namespace Ui
