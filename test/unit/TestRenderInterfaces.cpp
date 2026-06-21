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

/**
 * @file TestRenderInterfaces.cpp
 * @brief Compile + behavior guard for the framework's draw sink Ui::IRender and
 *        Ui::Render::shadow_t. Trivial mock implementations prove the
 *        pure-virtual signatures are implementable and that the interface
 *        speaks only in fw value types.
 */

#include "ui/interface/irender.h"
#include "ui/render/shadow.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace {

// Minimal IRender that records call counts - proves every primitive's signature
// compiles and is overridable through the base interface.
class MockRender final : public Ui::IRender {
public:
    int               frames   = 0;
    int               rects    = 0;
    int               texts    = 0;
    int               images   = 0;
    int               tris     = 0;
    Ui::font_handle_t nextFont = 1;

    void beginFrame(Ui::fpx_t, Ui::fpx_t) override { ++frames; }
    void endFrame() override { }
    void fillRect(const Ui::Res::Type::bound_t &,
                  const Ui::Res::Type::border_t &,
                  const Ui::Res::Type::color_pair_t &,
                  const Ui::Render::shadow_t &) override
    {
        ++rects;
    }
    void drawText(Ui::font_handle_t,
                  std::string_view,
                  const Ui::Res::Type::bound_t &,
                  const Ui::Color &,
                  bool,
                  Ui::fpx_t) override
    {
        ++texts;
    }
    void drawImage(std::string_view,
                   const Ui::Res::Type::bound_t &,
                   const Ui::Res::Type::border_t &,
                   const Ui::Color &,
                   Ui::fpx_t,
                   const Ui::Render::shadow_t &) override
    {
        ++images;
    }
    void drawTriangle(Ui::fpx_t, Ui::fpx_t, Ui::fpx_t, Ui::fpx_t, Ui::fpx_t, Ui::fpx_t, const Ui::Color &) override
    {
        ++tris;
    }

    Ui::fpx_t textWidth(Ui::font_handle_t, std::wstring_view text) override
    {
        return static_cast<Ui::fpx_t>(text.size());
    }
    Ui::font_handle_t createFont(const Ui::Res::Type::font_t &) override { return nextFont++; }
};

} // namespace

// ============================================================================
// shadow_t
// ============================================================================

TEST(Shadow, DefaultIsInvisible)
{
    Ui::Render::shadow_t s;
    EXPECT_FALSE(s.isVisible()); // opacity 0, transparent color
}

TEST(Shadow, VisibleNeedsOpacityAndAlpha)
{
    Ui::Render::shadow_t s;
    s.opacity = 0.5F;
    s.color   = Ui::Color { 0, 0, 0, 255 };
    EXPECT_TRUE(s.isVisible());
}

TEST(Shadow, InvisibleWhenOpacityZero)
{
    Ui::Render::shadow_t s;
    s.color = Ui::Color { 0, 0, 0, 255 }; // opaque color but opacity still 0
    EXPECT_FALSE(s.isVisible());
}

TEST(Shadow, InvisibleWhenColorTransparent)
{
    Ui::Render::shadow_t s;
    s.opacity = 1.0F;
    s.color   = Ui::Color { 0, 0, 0, 0 }; // fully transparent
    EXPECT_FALSE(s.isVisible());
}

TEST(Shadow, Equality)
{
    Ui::Render::shadow_t a;
    Ui::Render::shadow_t b;
    EXPECT_EQ(a, b);
    b.blur = 4.0F;
    EXPECT_NE(a, b);
}

// ============================================================================
// IRender dispatch through the base interface
// ============================================================================

TEST(RenderInterface, DispatchesThroughBase)
{
    MockRender    mock;
    Ui::IRender & render = mock;

    render.beginFrame(800, 600);
    render.fillRect({}, {}, {}, {});
    render.drawText(render.createFont({}), "hello", {}, Ui::Color {}, true, 0);
    render.drawImage("icon.svg", {}, {}, Ui::Color {}, 1.0F, {});
    render.drawTriangle(0, 0, 4, 2, 0, 4, Ui::Color {});
    render.endFrame();

    EXPECT_EQ(mock.frames, 1);
    EXPECT_EQ(mock.rects, 1);
    EXPECT_EQ(mock.texts, 1);
    EXPECT_EQ(mock.images, 1);
    EXPECT_EQ(mock.tris, 1);
    EXPECT_EQ(render.textWidth(1, L"abcd"), 4.0F);
}
