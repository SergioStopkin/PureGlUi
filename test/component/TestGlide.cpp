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
 * @file TestGlide.cpp
 * @brief Where and in which colours the main renderer draws what moves: a dragged tab
 *        under the pointer, held along the strip, over the other tabs in a layer of its own
 *        with its close glyph; let go, it glides to its place; a tab layout moves glides
 *        from where it was drawn; a hovered one fades to its hover colour; a dock row's
 *        selection fades in and out; a resize and a theme switch jump.
 *
 * Recorded through a fake IRender, so these assert where the renderer ASKS something to be
 * drawn, in which colour and which layer, frame by frame, with no window.
 */

#include "common/time.h"
#include "testconst.h"
#include "ui/animation/durationof.h"
#include "ui/color.h"
#include "ui/config.h"
#include "ui/interface/irender.h"
#include "ui/render/dockcolumn.h"
#include "ui/render/uirenderer.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/row.h"
#include "ui/res/dock/state.h"
#include "ui/res/key/element.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/tab.h"
#include "ui/type.h"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

namespace {

    // A fill or an image the renderer asked for, and its layer: 0 the frame's, 1 the held tab's
    struct alignas(64) drawn_t final {
        Ui::Res::Type::bound_t bound;
        Ui::Color              color;
        int                    layer       = 0;
        bool                   isOverDrawn = false; // a transparent ground: its corners show what is under it
    };

    // Records one frame's fills and images, each with the layer it was asked for in
    class LayerRender final : public Ui::IRender {
    public:
        std::vector<drawn_t> fills;
        std::vector<drawn_t> images;
        int                  layer = 0;

        void beginFrame(Ui::fpx_t /*width*/, Ui::fpx_t /*height*/) override
        {
            fills.clear();
            images.clear();
            layer = 0;
        }
        void endFrame() override { }
        void flushLayer() override { ++layer; }
        void fillRect(const Ui::Res::Type::bound_t & bound,
                      const Ui::Res::Type::border_t & /*radii*/,
                      const Ui::Res::Type::color_pair_t & colors,
                      const Ui::Render::shadow_t & /*shadow*/) override
        {
            fills.emplace_back(
            drawn_t { .bound = bound, .color = colors.fg, .layer = layer, .isOverDrawn = colors.bg.a() == 0 });
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
                       const Ui::Res::Type::bound_t & bound,
                       const Ui::Res::Type::border_t & /*radii*/,
                       const Ui::Color & tint,
                       Ui::fpx_t /*scale*/,
                       const Ui::Render::shadow_t & /*shadow*/,
                       bool /*isFilled*/) override
        {
            images.emplace_back(drawn_t { .bound = bound, .color = tint, .layer = layer });
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

        [[nodiscard]] Ui::font_handle_t createFont(const Ui::Res::Type::font_t & /*font*/) override
        {
            return ++m_nextFont;
        }

    private:
        Ui::font_handle_t m_nextFont = 0;
    };

    constexpr Ui::id_t  FIRST    = 1; // the active tab, the one every test moves
    constexpr Ui::id_t  SECOND   = 2;
    constexpr Ui::id_t  THIRD    = 3;
    constexpr Ui::id_t  ROW      = 4;        // a dock row, apart from every tab
    constexpr Ui::fpx_t NUDGE    = 10.0F;    // a drag that crosses no neighbour's middle
    constexpr Ui::fpx_t PAST_END = 10000.0F; // a drag past either end of the strip

} // namespace

class GlideTest : public ::testing::Test {
protected:
    Ui::Res::ResManager                     resManager { TEST_RES_DIR };
    std::unique_ptr<Ui::Render::UiRenderer> renderer;
    LayerRender *                           sink = nullptr;

    void SetUp() override
    {
        resManager.loadAll();
        project({ FIRST, SECOND, THIRD });
        auto render = std::make_unique<LayerRender>();
        sink        = render.get();
        renderer    = std::make_unique<Ui::Render::UiRenderer>(std::move(render), VIEW_W, VIEW_H, resManager);
        draw(VIEW_W);
    }

    // The tabs in this order, FIRST the active one, each with a file - so a close glyph
    void project(const std::vector<Ui::id_t> & ids)
    {
        std::vector<Ui::tab_t> tabs;
        for (const Ui::id_t id : ids) {
            tabs.emplace_back(Ui::tab_t { .id = id, .isActive = id == FIRST, .hasContent = true });
        }
        resManager.tabBar().setTabs(tabs);
    }

    void draw(Ui::fpx_t width)
    {
        renderer->setContent();
        renderer->Render(width, VIEW_H);
    }

    // The active tab's fill in the last frame: the one fill in its colour at a tab's size
    [[nodiscard]] drawn_t activeTabFill() const
    {
        const Ui::Res::Type::bound_t tab   = renderer->bound(FIRST);
        const Ui::Color &            color = resManager.theme().workspaceTabActive.bg;
        const auto                   it    = std::ranges::find_if(sink->fills, [&](const drawn_t & fill) {
            return fill.color == color && fill.bound.w == tab.w && fill.bound.h == tab.h;
        });
        if (it == sink->fills.end()) {
            ADD_FAILURE() << "the active tab was not drawn";
            return {};
        }
        return *it;
    }

    // The fill drawn last frame exactly at `bound` - a tab's, by where layout put it
    [[nodiscard]] drawn_t fillAt(const Ui::Res::Type::bound_t & bound) const
    {
        const auto it = std::ranges::find(sink->fills, bound, &drawn_t::bound);
        if (it == sink->fills.end()) {
            ADD_FAILURE() << "nothing was drawn there";
            return {};
        }
        return *it;
    }

    // How long a tab takes to glide its whole way, by the shipped workspace-tab transition
    [[nodiscard]] Common::Time::span_t longestGlide() const
    {
        return Ui::Animation::durationOf(resManager.transitionsFor(Ui::Res::Key::ElementKey::WorkspaceTab));
    }

    // How many fills of `color` the last frame drew inside `area`
    [[nodiscard]] std::ptrdiff_t fillsOf(const Ui::Color & color, const Ui::Res::Type::bound_t & area) const
    {
        return std::ranges::count_if(sink->fills, [&](const drawn_t & fill) {
            return fill.color == color && area.contains(fill.bound.x, fill.bound.y);
        });
    }
};

TEST_F(GlideTest, NothingHeldIsOneLayer)
{
    EXPECT_EQ(sink->layer, 0);
    EXPECT_FLOAT_EQ(activeTabFill().bound.x, renderer->bound(FIRST).x);
}

TEST_F(GlideTest, HeldTabDrawnWhereTheDragPutsItOverTheOthers)
{
    const Ui::Res::Type::bound_t second = renderer->bound(SECOND);
    renderer->holdTab(FIRST, second.x + NUDGE);
    draw(VIEW_W);
    const drawn_t held = activeTabFill();
    EXPECT_EQ(held.layer, 1);
    EXPECT_FLOAT_EQ(held.bound.x, second.x + NUDGE);
    EXPECT_TRUE(held.isOverDrawn);
    EXPECT_TRUE(std::ranges::any_of(sink->images, [](const drawn_t & image) { return image.layer == 1; }))
    << "its close glyph is drawn over the other tabs with it";
}

TEST_F(GlideTest, HeldTabKeptAlongTheStrip)
{
    const Ui::Res::Type::bound_t first = renderer->bound(FIRST);
    const Ui::Res::Type::bound_t third = renderer->bound(THIRD);
    renderer->holdTab(FIRST, first.x - PAST_END);
    draw(VIEW_W);
    EXPECT_FLOAT_EQ(activeTabFill().bound.x, first.x);
    renderer->holdTab(FIRST, third.x + PAST_END);
    draw(VIEW_W);
    EXPECT_FLOAT_EQ(activeTabFill().bound.x, third.x);
}

TEST_F(GlideTest, ReleasedTabGlidesToItsPlace)
{
    const Ui::Res::Type::bound_t first = renderer->bound(FIRST);
    const Ui::Res::Type::bound_t third = renderer->bound(THIRD);
    renderer->holdTab(FIRST, third.x);
    draw(VIEW_W);
    renderer->releaseTab();
    draw(VIEW_W);
    EXPECT_EQ(sink->layer, 0);
    EXPECT_FLOAT_EQ(activeTabFill().bound.x, third.x);
    EXPECT_TRUE(renderer->isGliding());

    Common::Time::sleepFor(longestGlide());
    draw(VIEW_W);
    EXPECT_FLOAT_EQ(activeTabFill().bound.x, first.x);
    EXPECT_FALSE(renderer->isGliding());
}

TEST_F(GlideTest, TabLayoutMovesGlidesFromWhereItWasDrawn)
{
    const Ui::Res::Type::bound_t first = renderer->bound(FIRST);
    project({ SECOND, FIRST, THIRD });
    draw(VIEW_W);
    EXPECT_GT(renderer->bound(FIRST).x, first.x);
    EXPECT_FLOAT_EQ(activeTabFill().bound.x, first.x);
    EXPECT_TRUE(renderer->isGliding());
}

TEST_F(GlideTest, HoveredTabFadesToItsHoverColour)
{
    const Ui::Res::Type::bound_t second = renderer->bound(SECOND);
    const auto &                 theme  = resManager.theme();
    std::ignore = renderer->onMouseMove(Ui::roundToInt(Ui::toPhys(second.x + (second.w / 2.0F))),
                                        Ui::roundToInt(Ui::toPhys(second.y + (second.h / 2.0F))));
    draw(VIEW_W);
    EXPECT_EQ(fillAt(second).color, theme.workspaceTab.bg);
    EXPECT_TRUE(renderer->isGliding());

    Common::Time::sleepFor(longestGlide());
    draw(VIEW_W);
    EXPECT_EQ(fillAt(second).color, theme.workspaceTabHover.bg);
    EXPECT_FALSE(renderer->isGliding());
}

TEST_F(GlideTest, DockRowSelectionFadesInAndOut)
{
    const std::string name = "glide-rows";
    resManager.setDockState(name, Ui::Res::Dock::dock_state_t { ROW_DOCK_WIDTH, ROW_DOCK_WIDTH });
    Ui::Render::DockColumn dock(
    1,
    Ui::Res::Dock::dock_config_t { name, Ui::Res::Dock::DockAnchor::Left, 1, ROW_DOCK_WIDTH },
    resManager);
    dock.setLayout(DOCK_TOP, ROW_DOCK_HEIGHT, ROW_DOCK_EDGE);
    renderer->setExtraOpsHook([&dock](Ui::Render::UiRenderer & out) { dock.render(out); });

    Ui::Res::Dock::row_t row;
    row.id    = ROW;
    row.label = "Part";
    dock.setRows({ row });
    draw(VIEW_W);

    const Ui::Color &          selected = resManager.theme().menuItemActive.bg;
    const Common::Time::span_t fade     = Ui::Animation::durationOf(
    resManager.transitionsFor(Ui::Res::Key::ElementKey::TopMenuItem));
    row.isSelected = true;
    dock.setRows({ row });
    draw(VIEW_W);
    EXPECT_EQ(fillsOf(selected, dock.content()), 0) << "it fades in from none";
    EXPECT_TRUE(renderer->isGliding());
    Common::Time::sleepFor(fade);
    draw(VIEW_W);
    EXPECT_EQ(fillsOf(selected, dock.content()), 1);

    row.isSelected = false;
    dock.setRows({ row });
    draw(VIEW_W);
    EXPECT_EQ(fillsOf(selected, dock.content()), 1) << "it fades out from the selection";
    Common::Time::sleepFor(fade);
    draw(VIEW_W);
    EXPECT_EQ(fillsOf(selected, dock.content()), 0);
}

// A theme switch is no change of state: every colour lands on the new theme's at once, as the
// grounds drawn around them do
TEST_F(GlideTest, ThemeSwitchJumps)
{
    const Ui::Color before = resManager.theme().workspaceTabActive.bg;
    resManager.switchThemeMode();
    ASSERT_NE(resManager.theme().workspaceTabActive.bg, before) << "the active tab is one colour in both modes";
    renderer->apply(resManager.changed());
    draw(VIEW_W);
    EXPECT_EQ(activeTabFill().color, resManager.theme().workspaceTabActive.bg);
    EXPECT_FALSE(renderer->isGliding());
}

TEST_F(GlideTest, ResizeJumps)
{
    project({ SECOND, FIRST, THIRD });
    draw(VIEW_W);
    ASSERT_TRUE(renderer->isGliding());
    renderer->resize(VIEW_W + NUDGE, VIEW_H);
    draw(VIEW_W + NUDGE);
    EXPECT_FLOAT_EQ(activeTabFill().bound.x, renderer->bound(FIRST).x);
    EXPECT_FALSE(renderer->isGliding());
}

} // namespace PureGlUi
