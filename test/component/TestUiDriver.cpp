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
 * @brief Headless UI-driver tests (Tier 1): exercise the real UI logic pipeline
 *        with no GL context and no real window.
 *
 * The app's UI behaviour is a pure chain -
 *   event -> hit-test(geometry) -> element id -> intent -> actionKey -> action
 * - and every stage here is GL-free or takes an injectable IRender (FakeRender).
 * Covers the addressing modes a UI test needs (by id / keyboard / coordinate),
 * the action tier, radio-highlight state, and popup-item geometry.
 */

#include "fakerender.h"
#include "ui/action/registry.h"
#include "ui/intent.h"
#include "ui/render/context.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/res/util.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

// Loads the real resource set once per test (menus, shortcuts, layout, theme).
// Pure C++: ResManager and Context never touch GL.
class UiDriverTest : public ::testing::Test {
protected:
    Ui::Res::ResManager resManager { TEST_RES_DIR };

    void SetUp() override { resManager.loadAll(); }

    // First top menu that opens a dropdown (no directly-bound action).
    [[nodiscard]] const Ui::Res::Type::menu_t * popupMenu() const
    {
        for (const auto & menu : resManager.menus()) {
            if (resManager.actionKeyFor(menu.id).empty() && !menu.items.empty()) {
                return &menu;
            }
        }
        return nullptr;
    }

    // All items (item + submenu levels) bound to actionKey, in menu order.
    [[nodiscard]] std::vector<const Ui::Res::Type::menu_t *> collectByAction(const std::string & actionKey) const
    {
        std::vector<const Ui::Res::Type::menu_t *> out;
        for (const auto & menu : resManager.menus()) {
            for (const auto & item : menu.items) {
                if (item.actionKey == actionKey && !item.separator && !item.label.empty()) {
                    out.push_back(&item);
                }
                for (const auto & sub : item.items) {
                    if (sub.actionKey == actionKey && !sub.separator && !sub.label.empty()) {
                        out.push_back(&sub);
                    }
                }
            }
        }
        return out;
    }
};

// -- Mode 1: address by element id -----------------------------------------
// Activating a top-menu button (no bound action) toggles its popup open/closed.
TEST_F(UiDriverTest, ActivateMenuButtonById_TogglesPopup)
{
    const Ui::Res::Type::menu_t * opener = popupMenu();
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
// A click at a menu button's centre resolves back to its id; empty space misses.
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

// -- Popup geometry: coordinate hit-test inside a built popup ---------------
// Closes the gap the main-bar hit-test leaves: buildPopup lays out dropdown
// items, and a click at an item's centre resolves to its id (a separator/gap
// does not). This is how a popup renderer resolves a click.
TEST_F(UiDriverTest, PopupItemHitTestResolvesToItemId)
{
    const Ui::Res::Type::menu_t * menu = popupMenu();
    ASSERT_NE(menu, nullptr);

    const std::vector<Ui::Render::UiElement> popup = Ui::Render::UiLayout::buildPopup(*menu, resManager);
    ASSERT_FALSE(popup.empty());

    const Ui::Render::UiElement * target = nullptr;
    for (const auto & element : popup) {
        if (element.type == Ui::Render::UiElementType::MenuItem) {
            target = &element;
            break;
        }
    }
    ASSERT_NE(target, nullptr) << "expected a clickable item in the popup";

    const Ui::fpx_t centerX = target->bound.x + target->bound.w / 2.0F;
    const Ui::fpx_t centerY = target->bound.y + target->bound.h / 2.0F;

    const Ui::Render::UiElement * hit = nullptr;
    for (const auto & element : popup) {
        if (element.type != Ui::Render::UiElementType::Separator && element.bound.contains(centerX, centerY)) {
            hit = &element;
            break;
        }
    }
    ASSERT_NE(hit, nullptr);
    EXPECT_EQ(hit->id, target->id);

    // Below the last row, nothing is hit.
    const Ui::fpx_t belowY = popup.back().bound.y + popup.back().bound.h + 50.0F;
    bool            missed = true;
    for (const auto & element : popup) {
        if (element.bound.contains(centerX, belowY)) {
            missed = false;
        }
    }
    EXPECT_TRUE(missed);
}

// -- Radio highlight: isActiveItem follows the action's value provider ------
// A stateful (radio) action highlights the item whose label equals the action's
// current value; switching the value moves the highlight. Fully headless.
TEST_F(UiDriverTest, RadioHighlightFollowsProvider)
{
    // Theme submenu items share actionKey "SwitchTheme"; each label is the value.
    const std::vector<const Ui::Res::Type::menu_t *> themeItems = collectByAction("SwitchTheme");
    if (themeItems.size() < 2) {
        GTEST_SKIP() << "need >= 2 SwitchTheme items";
    }

    std::string current;
    resManager.setActionValueProvider("SwitchTheme", [&current] { return current; });

    current = themeItems[0]->label;
    EXPECT_TRUE(resManager.isActiveItem(*themeItems[0]));
    EXPECT_FALSE(resManager.isActiveItem(*themeItems[1]));

    current = themeItems[1]->label; // switch: the highlight must follow the value
    EXPECT_FALSE(resManager.isActiveItem(*themeItems[0]));
    EXPECT_TRUE(resManager.isActiveItem(*themeItems[1]));
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
