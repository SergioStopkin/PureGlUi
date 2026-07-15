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
 * @file TestUiDriver.cpp
 * @brief Headless UI-driver prototype (Tier 1): exercise the real UI logic
 *        pipeline with no GL context and no real window.
 *
 * The app's UI behaviour is a pure chain -
 *   event -> hit-test(geometry) -> element id -> intent -> actionKey -> action
 * - and every stage here is GL-free or takes an injectable IRender. This suite
 * drives the three addressing modes a UI test needs, all deterministic and
 * platform-neutral (no display, no GL, no OS input injection):
 *
 *   1. by element id      - Context::mapClick (activate/open/close an element)
 *   2. by keyboard         - Util::strKey + Context::mapKey (shortcut -> action)
 *   3. by coordinate       - UiLayout::hitTest (click at x,y -> element id)
 *
 * plus the action tier: Action::Registry with a spy, closing the intent -> action
 * loop. FakeRender supplies deterministic font metrics so layout/bounds compute
 * without FreeType/GL being exercised at runtime.
 */

#include "ui/action/registry.h"
#include "ui/intent.h"
#include "ui/interface/irender.h"
#include "ui/render/context.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/res/util.h"
#include "ui/type.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

// Deterministic IRender: no drawing, fixed text metrics. Lets UiLayout compute
// bounds headlessly - the absolute widths are irrelevant, only that the same
// inputs always produce the same geometry, so hit-test mapping is verifiable.
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
                  bool /*centered*/,
                  Ui::fpx_t /*minPadH*/) override
    {
    }
    void drawImage(std::string_view /*src*/,
                   const Ui::Res::Type::bound_t & /*bound*/,
                   const Ui::Res::Type::border_t & /*radii*/,
                   const Ui::Color & /*tint*/,
                   Ui::fpx_t /*scale*/,
                   const Ui::Render::shadow_t & /*shadow*/) override
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

// Loads the real resource set once per test (menus, shortcuts, layout, theme).
// Pure C++: ResManager and Context never touch GL.
class UiDriverTest : public ::testing::Test {
protected:
    Ui::Res::ResManager resManager { TEST_RES_DIR };

    void SetUp() override { resManager.loadAll(); }
};

// -- Mode 1: address by element id -----------------------------------------
// Activating a top-menu button (no bound action) toggles its popup open/closed.
TEST_F(UiDriverTest, ActivateMenuButtonById_TogglesPopup)
{
    const auto & menus = resManager.menus();
    ASSERT_FALSE(menus.empty());

    // A top menu that opens a dropdown is one with no directly-bound action.
    const Ui::Res::Type::menu_t * opener = nullptr;
    for (const auto & menu : menus) {
        if (resManager.actionKeyFor(menu.id).empty()) {
            opener = &menu;
            break;
        }
    }
    ASSERT_NE(opener, nullptr) << "expected at least one popup-opening top menu";

    Ui::Render::Context        context(resManager);
    Ui::Render::click_result_t click;
    click.type = Ui::Render::UiElementType::MenuButton;
    click.id   = opener->id;

    // Closed -> click opens the popup for this id.
    const Ui::result_t opened = context.mapClick(click, Ui::INVALID_ID);
    ASSERT_EQ(opened.intents.size(), 1U);
    EXPECT_EQ(opened.intents.front().kind, Ui::IntentKind::OpenPopup);
    EXPECT_EQ(opened.intents.front().id, opener->id);

    // Open -> the same click closes it (toggle).
    const Ui::result_t closed = context.mapClick(click, opener->id);
    ASSERT_EQ(closed.intents.size(), 1U);
    EXPECT_EQ(closed.intents.front().kind, Ui::IntentKind::ClosePopup);
}

// -- Mode 2: address by keyboard shortcut ----------------------------------
// Every data-driven shortcut resolves to an EmitAction carrying its actionKey;
// an unknown combo resolves to nothing.
TEST_F(UiDriverTest, ShortcutResolvesToAction)
{
    // Normalization is what turns a res "Ctrl + O" into the map key.
    EXPECT_EQ(Ui::Res::Util::strKey("Ctrl + O"), "ctrl+o");

    const auto & shortcuts = resManager.shortcuts();
    ASSERT_FALSE(shortcuts.empty());

    Ui::Render::Context context(resManager);
    for (const auto & [normalizedKey, action] : shortcuts) {
        const Ui::result_t result = context.mapKey(normalizedKey);
        ASSERT_EQ(result.intents.size(), 1U) << "shortcut: " << normalizedKey;
        EXPECT_EQ(result.intents.front().kind, Ui::IntentKind::EmitAction);
        EXPECT_EQ(result.intents.front().actionKey, action);
    }

    EXPECT_TRUE(context.mapKey("ctrl+shift+f19").intents.empty());
}

// -- Mode 3: address by coordinate -----------------------------------------
// A click at an element's centre resolves back to that element's id; a click in
// empty space hits nothing. Verifies the hit-test/geometry mapping id-based
// testing skips.
TEST_F(UiDriverTest, HitTestCoordinateResolvesToElementId)
{
    FakeRender           render;
    Ui::Render::UiLayout layout;
    layout.build(resManager.layout(),
                 resManager.theme(),
                 resManager.menus(),
                 resManager.buttons(),
                 resManager.tabBar(),
                 resManager.localeManager(),
                 1600.0F,
                 1000.0F,
                 &render);
    ASSERT_FALSE(layout.elements().empty());

    const Ui::Render::UiElement * target = nullptr;
    for (const auto & element : layout.elements()) {
        if (element.type == Ui::Render::UiElementType::MenuButton) {
            target = &element;
            break;
        }
    }
    ASSERT_NE(target, nullptr) << "expected a top-menu button in the layout";

    const Ui::fpx_t centerX = target->bound.x + target->bound.w / 2.0F;
    const Ui::fpx_t centerY = target->bound.y + target->bound.h / 2.0F;

    const Ui::Render::UiElement * hit = layout.hitTest(centerX, centerY);
    ASSERT_NE(hit, nullptr);
    EXPECT_EQ(hit->id, target->id);
    EXPECT_EQ(hit->type, target->type);

    // A coordinate far outside every element hits nothing.
    EXPECT_EQ(layout.hitTest(100000.0F, 100000.0F), nullptr);
}

// -- Action tier: intent -> registered handler -----------------------------
// Closes the loop: an EmitAction intent dispatches to a registered spy with its
// arg; an unregistered key is reported unhandled (drives the menu-greying pass).
TEST_F(UiDriverTest, EmitActionIntentDispatchesToHandler)
{
    Ui::Action::Registry registry;
    std::string          firedKey;
    std::string          firedArg;
    registry.on("SwitchTheme", [&](const std::string & arg) {
        firedKey = "SwitchTheme";
        firedArg = arg;
    });

    const Ui::intent_t intent { Ui::IntentKind::EmitAction, Ui::INVALID_ID, "SwitchTheme", "emerald" };

    ASSERT_EQ(intent.kind, Ui::IntentKind::EmitAction);
    ASSERT_TRUE(registry.has(intent.actionKey));
    registry.dispatch(intent.actionKey, intent.arg);

    EXPECT_EQ(firedKey, "SwitchTheme");
    EXPECT_EQ(firedArg, "emerald");
    EXPECT_FALSE(registry.has("NoSuchAction"));
}

} // namespace PureGlUi
