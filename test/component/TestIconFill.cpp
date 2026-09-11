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
 * @file TestIconFill.cpp
 * @brief Tests for the isFilled flag that reaches the draw sink with every image.
 *
 * Nearly every shipped icon is authored as an outline (fill="none"); the GL
 * backend can force it solid by loading a rewritten variant. Which of the two an
 * icon gets is a DESIGN decision, so it travels with the op rather than being
 * decided down in the backend: false renders the file as authored, true fills it.
 *
 * The flag is recorded through a fake IRender, so these assert what the renderer
 * ASKS FOR - the only part the backend cannot second-guess.
 */

#include "ui/interface/irender.h"
#include "ui/render/uirenderer.h"
#include "ui/res/resmanager.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

namespace {

    // Records every drawImage/warmImage request, keeping the flag under test
    struct alignas(64) image_call_t final {
        std::string src;
        bool        isFilled = false;
    };

    class RecordingRender final : public Ui::IRender {
    public:
        std::vector<image_call_t> images;
        std::vector<image_call_t> warms;

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
        void drawImage(std::string_view src,
                       const Ui::Res::Type::bound_t & /*bound*/,
                       const Ui::Res::Type::border_t & /*radii*/,
                       const Ui::Color & /*tint*/,
                       Ui::fpx_t /*scale*/,
                       const Ui::Render::shadow_t & /*shadow*/,
                       bool isFilled) override
        {
            images.emplace_back(image_call_t { std::string(src), isFilled });
        }
        void warmImage(std::string_view src,
                       const Ui::Res::Type::bound_t & /*bound*/,
                       const Ui::Color & /*tint*/,
                       Ui::fpx_t /*scale*/,
                       bool isFilled) override
        {
            warms.emplace_back(image_call_t { std::string(src), isFilled });
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
            return static_cast<Ui::fpx_t>(text.size()) * 8.0F;
        }

        [[nodiscard]] Ui::font_handle_t createFont(const Ui::Res::Type::font_t & /*font*/) override
        {
            return ++m_nextFont;
        }

    private:
        Ui::font_handle_t m_nextFont = 0;
    };

    constexpr Ui::fpx_t VIEW_W = 1200.0F;
    constexpr Ui::fpx_t VIEW_H = 800.0F;

} // namespace

class IconFillTest : public ::testing::Test {
protected:
    Ui::Res::ResManager resManager { TEST_RES_DIR };

    void SetUp() override { resManager.loadAll(); }

    // A UiRenderer over the recording sink, rendered once. The sink is owned by
    // the renderer, so the pointer is kept for the assertions.
    RecordingRender & render(std::unique_ptr<Ui::Render::UiRenderer> & outRenderer)
    {
        auto   sink    = std::make_unique<RecordingRender>();
        auto * watched = sink.get();
        outRenderer    = std::make_unique<Ui::Render::UiRenderer>(std::move(sink), VIEW_W, VIEW_H, resManager);
        outRenderer->setContent();
        outRenderer->Render(VIEW_W, VIEW_H);
        return *watched;
    }
};

// ============================================================================
// Default: as authored
// ============================================================================

TEST_F(IconFillTest, IconsAreDrawnAsAuthoredByDefault)
{
    std::unique_ptr<Ui::Render::UiRenderer> renderer;
    const RecordingRender &                 sink = render(renderer);

    ASSERT_FALSE(sink.images.empty()) << "the shipped res set draws no icon at all";
    for (const auto & call : sink.images) {
        EXPECT_FALSE(call.isFilled) << "unexpected forced fill for " << call.src;
    }
}

// The warm pass keys the same texture cache as the draw, so a mismatch would
// warm a variant nothing ever draws
TEST_F(IconFillTest, WarmedIconsMatchTheDrawnVariant)
{
    std::unique_ptr<Ui::Render::UiRenderer> renderer;
    const RecordingRender &                 sink = render(renderer);

    for (const auto & warm : sink.warms) {
        for (const auto & drawn : sink.images) {
            if (drawn.src == warm.src) {
                EXPECT_EQ(warm.isFilled, drawn.isFilled) << "warm/draw disagree for " << warm.src;
            }
        }
    }
}

// ============================================================================
// Opt-in: a solid mark asks for it explicitly
// ============================================================================

TEST_F(IconFillTest, AppendImageCarriesTheCallersChoiceBothWays)
{
    std::unique_ptr<Ui::Render::UiRenderer> renderer;
    auto                                    sink    = std::make_unique<RecordingRender>();
    auto *                                  watched = sink.get();
    renderer = std::make_unique<Ui::Render::UiRenderer>(std::move(sink), VIEW_W, VIEW_H, resManager);
    renderer->setContent();

    const std::string icon = resManager.resPath().icon(resManager.layout().dockDefaults.gripIcon);
    renderer->setExtraOpsHook([&icon](Ui::Render::UiRenderer & out) {
        out.appendImage({ 0, 0, 10, 10 }, icon, Ui::Color(255, 255, 255), true);
        out.appendImage({ 0, 20, 10, 10 }, icon, Ui::Color(255, 255, 255));
    });
    renderer->Render(VIEW_W, VIEW_H);

    int filled = 0;
    int asIs   = 0;
    for (const auto & call : watched->images) {
        if (call.src != icon) {
            continue;
        }
        if (call.isFilled) {
            ++filled;
        } else {
            ++asIs;
        }
    }
    EXPECT_EQ(filled, 1);
    EXPECT_EQ(asIs, 1);
}

} // namespace PureGlUi
