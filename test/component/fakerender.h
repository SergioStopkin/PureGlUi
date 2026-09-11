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

#include "ui/interface/irender.h"

#include <string_view>

namespace PureGlUi {

// Deterministic IRender for headless tests: no drawing, fixed text metrics, so
// UiLayout computes bounds without FreeType/GL. The absolute widths are
// irrelevant - only that identical inputs yield identical geometry, so hit-test
// mapping is verifiable. Shared by the headless UI-test suites.
class FakeRender final : public Ui::IRender {
public:
    static constexpr Ui::fpx_t GLYPH_WIDTH = 8.0F;

    void beginFrame(Ui::fpx_t /*width*/, Ui::fpx_t /*height*/) override { }
    void endFrame() override { }
    void fillRect(const Ui::Res::Type::bound_t & /*bound*/,
                  const Ui::Res::Type::border_t & /*radii*/,
                  const Ui::Res::Type::color_pair_t & /*colors*/,
                  const Ui::Render::shadow_t & /*shadow*/) override
    {
    }
    void drawText(Ui::font_handle_t /*font*/,
                  std::string_view /*text*/,
                  const Ui::Res::Type::bound_t & /*pos*/,
                  const Ui::Color & /*color*/,
                  Ui::Res::Type::AlignH /*alignH*/,
                  Ui::Res::Type::AlignV /*alignV*/,
                  Ui::fpx_t /*minPadH*/) override
    {
    }
    void drawImage(std::string_view /*src*/,
                   const Ui::Res::Type::bound_t & /*bound*/,
                   const Ui::Res::Type::border_t & /*radii*/,
                   const Ui::Color & /*tint*/,
                   Ui::fpx_t /*scale*/,
                   const Ui::Render::shadow_t & /*shadow*/,
                   bool /*isFilled*/) override
    {
    }
    void warmImage(std::string_view /*src*/,
                   const Ui::Res::Type::bound_t & /*bound*/,
                   const Ui::Color & /*tint*/,
                   Ui::fpx_t /*scale*/,
                   bool /*isFilled*/) override
    {
    }
    void drawTriangle(Ui::fpx_t /*x0*/,
                      Ui::fpx_t /*y0*/,
                      Ui::fpx_t /*x1*/,
                      Ui::fpx_t /*y1*/,
                      Ui::fpx_t /*x2*/,
                      Ui::fpx_t /*y2*/,
                      const Ui::Color & /*color*/) override
    {
    }

    [[nodiscard]] Ui::fpx_t textWidth(Ui::font_handle_t /*font*/, std::wstring_view text) override
    {
        return static_cast<Ui::fpx_t>(text.size()) * GLYPH_WIDTH;
    }

    [[nodiscard]] Ui::font_handle_t createFont(const Ui::Res::Type::font_t & /*font*/) override { return ++m_nextFont; }

private:
    Ui::font_handle_t m_nextFont = 0;
};

} // namespace PureGlUi
