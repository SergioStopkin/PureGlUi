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
 * @file TestDockRowLayout.cpp
 * @brief Tests for how a dock row splits into key and value: the row minus its
 *        padding is halved, the key column pays for the expander and the depth
 *        indent out of its own half, and the value is right-aligned in the rest.
 *        Also which rows are offered to hit-testing at all, and that a row click
 *        reaches the host carrying the host's own row id.
 *
 * Text ops are recorded through a fake sink, so these assert the geometry the
 * renderer asks for. The label is what gets cut when a value needs more than its
 * half - a rounded number must never lose digits.
 */

#include "ui/elementid.h"
#include "ui/intent.h"
#include "ui/intentkind.h"
#include "ui/interface/irender.h"
#include "ui/render/context.h"
#include "ui/render/dockcolumn.h"
#include "ui/render/elementevent.h"
#include "ui/render/eventkind.h"
#include "ui/render/uielement.h"
#include "ui/render/uilayout.h"
#include "ui/render/uirenderer.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/row.h"
#include "ui/res/dock/state.h"
#include "ui/res/resmanager.h"
#include "ui/result.h"

#include <algorithm>
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

    struct alignas(64) text_call_t final {
        std::string            text;
        Ui::Res::Type::bound_t pos;
        Ui::Res::Type::AlignH  alignH = Ui::Res::Type::AlignH::Left;
    };

    // Fixed metrics so a character is worth a known width and the expectations are
    // arithmetic a reader can redo
    class RecordingText final : public Ui::IRender {
    public:
        static constexpr Ui::fpx_t GLYPH = 8.0F;

        std::vector<text_call_t> texts;

        void beginFrame(Ui::fpx_t /*width*/, Ui::fpx_t /*height*/) override { }
        void endFrame() override { }
        void fillRect(const Ui::Res::Type::bound_t & /*bound*/,
                      const Ui::Res::Type::border_t & /*radii*/,
                      const Ui::Res::Type::color_pair_t & /*colors*/,
                      const Ui::Render::shadow_t & /*shadow*/) override
        {
        }
        void drawText(Ui::font_handle_t /*font*/,
                      std::string_view               text,
                      const Ui::Res::Type::bound_t & pos,
                      const Ui::Color & /*color*/,
                      Ui::Res::Type::AlignH alignH,
                      Ui::Res::Type::AlignV /*alignV*/,
                      Ui::fpx_t /*minPadH*/) override
        {
            texts.emplace_back(text_call_t { std::string(text), pos, alignH });
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
            return static_cast<Ui::fpx_t>(text.size()) * GLYPH;
        }

        [[nodiscard]] Ui::font_handle_t createFont(const Ui::Res::Type::font_t & /*font*/) override
        {
            return ++m_nextFont;
        }

    private:
        Ui::font_handle_t m_nextFont = 0;
    };

    constexpr Ui::fpx_t VIEW_W     = 1200.0F;
    constexpr Ui::fpx_t VIEW_H     = 800.0F;
    constexpr Ui::fpx_t DOCK_TOP   = 100.0F;
    constexpr Ui::fpx_t DOCK_H     = 300.0F;
    constexpr Ui::fpx_t DOCK_EDGE  = 400.0F;
    constexpr Ui::fpx_t DOCK_WIDTH = 200.0F;

} // namespace

class DockRowLayoutTest : public ::testing::Test {
protected:
    Ui::Res::ResManager resManager { TEST_RES_DIR };

    void SetUp() override { resManager.loadAll(); }

    Ui::Res::Dock::dock_config_t seed(const std::string & name)
    {
        resManager.setDockState(name, Ui::Res::Dock::dock_state_t { DOCK_WIDTH, DOCK_WIDTH });
        return Ui::Res::Dock::dock_config_t { name, Ui::Res::Dock::DockAnchor::Left, 1, DOCK_WIDTH };
    }

    // Render one dock's rows through a recording sink and hand back what it drew
    std::vector<text_call_t> draw(Ui::Render::DockColumn & dock)
    {
        auto   sink    = std::make_unique<RecordingText>();
        auto * watched = sink.get();

        Ui::Render::UiRenderer renderer(std::move(sink), VIEW_W, VIEW_H, resManager);
        renderer.setContent();
        renderer.setExtraOpsHook([&dock](Ui::Render::UiRenderer & out) { dock.render(out); });
        renderer.Render(VIEW_W, VIEW_H);
        return watched->texts;
    }

    // The op carrying `text`, or nothing if it was cut away entirely
    static const text_call_t * find(const std::vector<text_call_t> & texts, std::string_view text)
    {
        const auto it = std::find_if(texts.begin(), texts.end(), [text](const text_call_t & call) {
            return call.text == text;
        });
        return it == texts.end() ? nullptr : &*it;
    }

    // What a left click at the middle of the first `type` element maps to. Hit
    // tested first, so whatever lies on top at that point takes the click - the
    // path a real press goes through
    [[nodiscard]] Ui::result_t clickMiddleOf(Ui::Render::UiLayout & layout, Ui::Render::UiElementType type) const
    {
        const auto & elements = layout.elements();
        const auto   target = std::find_if(elements.begin(), elements.end(), [type](const Ui::Render::UiElement & el) {
            return el.type == type;
        });
        if (target == elements.end()) {
            return {};
        }
        const Ui::fpx_t               x   = target->bound.x + (target->bound.w / 2.0F);
        const Ui::fpx_t               y   = target->bound.y + (target->bound.h / 2.0F);
        const Ui::Render::UiElement * hit = layout.hitTest(x, y, Ui::Render::EventKind::LeftClick);
        if (hit == nullptr) {
            return {};
        }
        Ui::Render::element_event_t click;
        click.type  = hit->type;
        click.id    = hit->id;
        click.event = Ui::Render::EventKind::LeftClick;
        return Ui::Render::Context(resManager).mapClick(click, Ui::INVALID_ID);
    }

    [[nodiscard]] Ui::fpx_t midpoint(const Ui::Render::DockColumn & dock) const
    {
        const Ui::fpx_t padH = resManager.popup().itemPaddingH;
        const auto &    row  = dock.content();
        return row.x + padH + ((row.w - (padH * 2.0F)) * resManager.layout().dockDefaults.rowKeyRatio);
    }
};

// ============================================================================
// The split
// ============================================================================

TEST_F(DockRowLayoutTest, KeyStaysInItsHalfAndValueIsRightAligned)
{
    Ui::Render::DockColumn dock(1, seed("row-split"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t row;
    row.label = "Area";
    row.value = "1.000 mm2";
    row.depth = 1;
    dock.setRows({ row });

    const auto          texts = draw(dock);
    const text_call_t * key   = find(texts, "Area");
    const text_call_t * value = find(texts, "1.000 mm2");
    ASSERT_NE(key, nullptr);
    ASSERT_NE(value, nullptr);

    EXPECT_LE(key->pos.x + key->pos.w, midpoint(dock) + 0.01F);
    EXPECT_EQ(value->alignH, Ui::Res::Type::AlignH::Right);
    EXPECT_GE(value->pos.x + value->pos.w, midpoint(dock));
}

// The key column pays for its own indent, so the split does not walk right as
// rows get deeper
TEST_F(DockRowLayoutTest, DeeperRowsSpendTheirOwnHalfOnTheIndent)
{
    Ui::Render::DockColumn dock(1, seed("row-depth"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t shallow;
    shallow.label             = "X";
    shallow.value             = "1";
    Ui::Res::Dock::row_t deep = shallow;
    deep.label                = "Y";
    deep.depth                = 2;
    dock.setRows({ shallow, deep });

    const auto          texts = draw(dock);
    const text_call_t * first = find(texts, "X");
    const text_call_t * later = find(texts, "Y");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(later, nullptr);

    EXPECT_GT(later->pos.x, first->pos.x);                          // indented
    EXPECT_LE(later->pos.x + later->pos.w, midpoint(dock) + 0.01F); // still inside its half
}

TEST_F(DockRowLayoutTest, LabelStartsAfterTheExpanderColumnWithAGap)
{
    Ui::Render::DockColumn dock(1, seed("row-gap"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t row;
    row.label = "Name";
    dock.setRows({ row });

    const auto          texts = draw(dock);
    const text_call_t * key   = find(texts, "Name");
    ASSERT_NE(key, nullptr);

    const Ui::fpx_t padH     = resManager.popup().itemPaddingH;
    const Ui::fpx_t expander = resManager.popup().itemHeight * resManager.layout().dockDefaults.rowExpanderRatio;
    // Padding + the expander column at least, plus the one-space gap
    EXPECT_GT(key->pos.x, dock.content().x + padH + expander);
}

// ============================================================================
// Who gives way
// ============================================================================

// The key keeps its half whatever the value does, so a long TEXT value is the
// one that gives way - it may spend the key's unused remainder, never its text
TEST_F(DockRowLayoutTest, ALongValueIsCutRatherThanOverrunningTheKey)
{
    Ui::Render::DockColumn dock(1, seed("row-long"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    const std::string    longValue = "a-very-long-part-name-that-cannot-possibly-fit";
    Ui::Res::Dock::row_t row;
    row.label = "Name";
    row.value = longValue;
    row.depth = 1;
    dock.setRows({ row });

    const auto texts = draw(dock);
    EXPECT_NE(find(texts, "Name"), nullptr) << "the key column is never given up";
    EXPECT_EQ(find(texts, longValue), nullptr) << "the value should have been cut to what is left";

    // And nothing drawn on the key side reaches past the split
    for (const auto & call : texts) {
        if (call.alignH != Ui::Res::Type::AlignH::Right) {
            EXPECT_LE(call.pos.x + call.pos.w, midpoint(dock) + 0.01F);
        }
    }
}

// Negative: a tree row has no value, so the key may use the whole row rather
// than being confined to half of it for no reason
TEST_F(DockRowLayoutTest, ARowWithNoValueDrawsOnlyItsLabel)
{
    Ui::Render::DockColumn dock(1, seed("row-tree"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t row;
    row.label = "Node";
    dock.setRows({ row });

    const auto texts = draw(dock);
    ASSERT_NE(find(texts, "Node"), nullptr);
    for (const auto & call : texts) {
        EXPECT_NE(call.alignH, Ui::Res::Type::AlignH::Right);
    }
}

// Negative: a collapsed dock has no content area, so it draws no row text at all
TEST_F(DockRowLayoutTest, ACollapsedDockDrawsNoRows)
{
    resManager.setDockState("row-collapsed", Ui::Res::Dock::dock_state_t { 0.0F, DOCK_WIDTH });
    Ui::Render::DockColumn dock(
    1,
    Ui::Res::Dock::dock_config_t { "row-collapsed", Ui::Res::Dock::DockAnchor::Left, 1, DOCK_WIDTH },
    resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t row;
    row.label = "Hidden";
    row.value = "1";
    dock.setRows({ row });

    const auto texts = draw(dock);
    EXPECT_EQ(find(texts, "Hidden"), nullptr);
    EXPECT_EQ(find(texts, "1"), nullptr);
}

// ============================================================================
// What a row offers to hit-testing
// ============================================================================

// A row with no id renders but never reports, so it offers no element at all.
// The dock-row offset wraps INVALID_ID into an id that looks real, and a click on
// it came back to the host as activateRow(INVALID_ID)
TEST_F(DockRowLayoutTest, ARowWithNoIdOffersNoElement)
{
    Ui::Render::DockColumn dock(1, seed("row-inert"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t selectable;
    selectable.id    = 3;
    selectable.label = "Part";
    Ui::Res::Dock::row_t inert;
    inert.label = "Area";
    inert.value = "1.000 mm2";
    dock.setRows({ selectable, inert });

    Ui::Render::UiLayout layout;
    dock.appendElements(layout);

    std::vector<Ui::id_t> rowIds;
    for (const Ui::Render::UiElement & element : layout.elements()) {
        if (element.type == Ui::Render::UiElementType::DockRow) {
            rowIds.emplace_back(element.id);
        }
    }
    EXPECT_EQ(rowIds, std::vector<Ui::id_t> { Ui::toDockRowElementId(selectable.id) });
}

// ============================================================================
// Row clicks reach the host with its own id
// ============================================================================

// The dock offsets a row id into the reserved range and Context takes it off
// again - two layers, so only the round trip shows the host gets back what it sent
TEST_F(DockRowLayoutTest, ARowClickActivatesTheHostsOwnId)
{
    Ui::Render::DockColumn dock(1, seed("row-activate"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t row;
    row.id          = 42;
    row.label       = "Part";
    row.hasChildren = true;
    dock.setRows({ row });

    Ui::Render::UiLayout layout;
    dock.appendElements(layout);

    const Ui::result_t              result = clickMiddleOf(layout, Ui::Render::UiElementType::DockRow);
    const std::vector<Ui::intent_t> expected { Ui::intent_t { Ui::IntentKind::ActivateRow, 42, {}, {} } };
    EXPECT_EQ(result.intents, expected);
}

// The expander lies on top of its row, so a click on it toggles the row instead
// of selecting it - and names the same host row
TEST_F(DockRowLayoutTest, AnExpanderClickTogglesTheSameRow)
{
    Ui::Render::DockColumn dock(1, seed("row-toggle"), resManager);
    dock.setLayout(DOCK_TOP, DOCK_H, DOCK_EDGE);

    Ui::Res::Dock::row_t row;
    row.id          = 42;
    row.label       = "Part";
    row.hasChildren = true;
    dock.setRows({ row });

    Ui::Render::UiLayout layout;
    dock.appendElements(layout);

    const Ui::result_t              result = clickMiddleOf(layout, Ui::Render::UiElementType::DockExpander);
    const std::vector<Ui::intent_t> expected { Ui::intent_t { Ui::IntentKind::ToggleRow, 42, {}, {} } };
    EXPECT_EQ(result.intents, expected);
}

} // namespace PureGlUi
