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
 * @file TestChromeLifecycle.cpp
 * @brief Chrome-lifecycle tests: click -> intent -> window/chrome command,
 *        headless (no GL window, no FakeWindow, no WindowManager).
 *
 * Drives the SAME code as the app: the real Ui::Render::Context (click -> intent)
 * and the real Ui::routeIntent (intent -> IChromeCommands command) that
 * Shell::execute() delegates to. Only the command sink differs - a RecordingChrome
 * double stands in for Shell's IChromeCommands impl. So there is one copy of the
 * routing, shared by app and test. The combined flow additionally resolves clicks
 * by coordinate via UiLayout (main bar) + buildPopup (dropdown), using FakeRender.
 */

#include "fakerender.h"
#include "recordingchrome.h"
#include "ui/intent.h"
#include "ui/interface/ichromecommands.h"
#include "ui/render/clickresult.h"
#include "ui/render/context.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

class ChromeLifecycleTest : public ::testing::Test {
protected:
    Ui::Res::ResManager resManager { TEST_RES_DIR };
    RecordingChrome     chrome;

    void SetUp() override { resManager.loadAll(); }

    // Drive a click through the REAL Context, then the REAL router - the same two
    // stages Shell::dispatchClick() runs. openMenuId is read from the chrome (its
    // single source), matching how Shell reads m_openMenuId.
    void click(Ui::Render::UiElementType type, Ui::id_t id)
    {
        Ui::Render::Context        context(resManager);
        Ui::Render::click_result_t hit;
        hit.type                  = type;
        hit.id                    = id;
        const Ui::result_t result = context.mapClick(hit, chrome.openMenuId);
        for (const Ui::intent_t & intent : result.intents) {
            Ui::routeIntent(intent, chrome);
        }
    }

    // Top menus that open a dropdown (no directly-bound action), in menu order.
    [[nodiscard]] std::vector<const Ui::Res::Type::menu_t *> popupMenus() const
    {
        std::vector<const Ui::Res::Type::menu_t *> out;
        for (const auto & menu : resManager.menus()) {
            if (resManager.actionKeyFor(menu.id).empty()) {
                out.push_back(&menu);
            }
        }
        return out;
    }

    // First leaf item (under a popup-opening top menu) matching a predicate,
    // paired with its parent menu so tests can open it the way a user would.
    template <class Pred>
    [[nodiscard]] std::pair<const Ui::Res::Type::menu_t *, const Ui::Res::Type::menu_t *> findLeaf(Pred pred) const
    {
        for (const auto & menu : resManager.menus()) {
            if (!resManager.actionKeyFor(menu.id).empty()) {
                continue; // not a popup opener
            }
            for (const auto & item : menu.items) {
                if (!item.separator && item.items.empty() && pred(item)) {
                    return { &menu, &item };
                }
            }
        }
        return { nullptr, nullptr };
    }
};

// Top menu opens a popup, then a second click on it closes - the core lifecycle.
TEST_F(ChromeLifecycleTest, TopMenuOpensThenCloses)
{
    const std::vector<const Ui::Res::Type::menu_t *> menus = popupMenus();
    ASSERT_FALSE(menus.empty());
    const Ui::id_t id = menus.front()->id;

    click(Ui::Render::UiElementType::MenuButton, id);
    click(Ui::Render::UiElementType::MenuButton, id);

    EXPECT_EQ(chrome.log, (std::vector<std::string> { "openPopup(" + std::to_string(id) + ")", "closePopup" }));
    EXPECT_EQ(chrome.openMenuId, Ui::INVALID_ID);
}

// Clicking a different menu while one is open switches (opens the new one) rather
// than closing - because click.id != openMenuId. A third click then toggles shut.
TEST_F(ChromeLifecycleTest, SwitchingMenusOpensNextThenToggles)
{
    const std::vector<const Ui::Res::Type::menu_t *> menus = popupMenus();
    if (menus.size() < 2) {
        GTEST_SKIP() << "need >= 2 popup-opening top menus";
    }

    click(Ui::Render::UiElementType::MenuButton, menus[0]->id); // open A
    click(Ui::Render::UiElementType::MenuButton, menus[1]->id); // click B while A open -> switch to B
    click(Ui::Render::UiElementType::MenuButton, menus[1]->id); // click B again -> close

    EXPECT_EQ(chrome.log,
              (std::vector<std::string> { "openPopup(" + std::to_string(menus[0]->id) + ")",
                                          "openPopup(" + std::to_string(menus[1]->id) + ")",
                                          "closePopup" }));
    EXPECT_EQ(chrome.openMenuId, Ui::INVALID_ID);
}

// Opening a menu then clicking a leaf action item closes the popup, then
// dispatches its action with the item label as the arg (parameterized actions).
TEST_F(ChromeLifecycleTest, LeafActionClosesPopupAndDispatches)
{
    const auto [parent, item] = findLeaf(
    [](const Ui::Res::Type::menu_t & i) { return !i.actionKey.empty() && i.dialog.title.empty(); });
    if (item == nullptr) {
        GTEST_SKIP() << "no leaf action item in res menus";
    }

    click(Ui::Render::UiElementType::MenuButton, parent->id);
    click(Ui::Render::UiElementType::MenuItem, item->id);

    EXPECT_EQ(chrome.log,
              (std::vector<std::string> { "openPopup(" + std::to_string(parent->id) + ")",
                                          "closePopup",
                                          "emitAction(" + item->actionKey + "," + item->label + ")" }));
}

// Opening a menu then clicking a leaf dialog item closes the popup, then opens
// its dialog (resolved by item id).
TEST_F(ChromeLifecycleTest, LeafDialogClosesPopupAndOpensDialog)
{
    const auto [parent, item] = findLeaf([](const Ui::Res::Type::menu_t & i) { return !i.dialog.title.empty(); });
    if (item == nullptr) {
        GTEST_SKIP() << "no leaf dialog item in res menus";
    }

    click(Ui::Render::UiElementType::MenuButton, parent->id);
    click(Ui::Render::UiElementType::MenuItem, item->id);

    EXPECT_EQ(chrome.log,
              (std::vector<std::string> { "openPopup(" + std::to_string(parent->id) + ")",
                                          "closePopup",
                                          "openDialog(" + std::to_string(item->id) + ")" }));
}

// A workspace tab click switches; its close button closes - both via host hooks.
TEST_F(ChromeLifecycleTest, TabClickSwitchesAndCloseCloses)
{
    click(Ui::Render::UiElementType::Tab, 42);
    click(Ui::Render::UiElementType::TabClose, 42);
    EXPECT_EQ(chrome.log, (std::vector<std::string> { "switchTab(42)", "closeTab(42)" }));
}

// Clicking the status-bar text copies it to the clipboard.
TEST_F(ChromeLifecycleTest, StatusTextCopies)
{
    resManager.setStatusText("hello world");
    click(Ui::Render::UiElementType::Text, Ui::INVALID_ID);
    EXPECT_EQ(chrome.log, (std::vector<std::string> { "copyText(hello world)" }));
}

// Full flow: resolve the menu button by coordinate (main layout), open its popup,
// resolve a leaf item by coordinate (popup layout), click it - closing the popup
// and dispatching. Chains hit-test + routing + state across both layouts.
TEST_F(ChromeLifecycleTest, FullFlowMenuBarClickToItemAction)
{
    const auto [parent, item] = findLeaf(
    [](const Ui::Res::Type::menu_t & i) { return !i.actionKey.empty() && i.dialog.title.empty(); });
    if (item == nullptr) {
        GTEST_SKIP() << "no leaf action item in res menus";
    }

    // (a) main layout: resolve the parent menu button by coordinate.
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
    const Ui::Render::UiElement * button = nullptr;
    for (const auto & element : layout.elements()) {
        if (element.type == Ui::Render::UiElementType::MenuButton && element.id == parent->id) {
            button = &element;
            break;
        }
    }
    ASSERT_NE(button, nullptr);
    const Ui::Render::UiElement * barHit = layout.hitTest(button->bound.x + button->bound.w / 2.0F,
                                                          button->bound.y + button->bound.h / 2.0F);
    ASSERT_NE(barHit, nullptr);
    ASSERT_EQ(barHit->id, parent->id);

    // (b) click it -> popup opens.
    click(Ui::Render::UiElementType::MenuButton, barHit->id);

    // (c) build the popup, resolve the leaf item by coordinate.
    const std::vector<Ui::Render::UiElement> popup     = Ui::Render::UiLayout::buildPopup(*parent, resManager);
    const Ui::Render::UiElement *            popupItem = nullptr;
    for (const auto & element : popup) {
        if (element.type == Ui::Render::UiElementType::MenuItem && element.id == item->id) {
            popupItem = &element;
            break;
        }
    }
    ASSERT_NE(popupItem, nullptr);
    const Ui::fpx_t               px       = popupItem->bound.x + popupItem->bound.w / 2.0F;
    const Ui::fpx_t               py       = popupItem->bound.y + popupItem->bound.h / 2.0F;
    const Ui::Render::UiElement * popupHit = nullptr;
    for (const auto & element : popup) {
        if (element.type != Ui::Render::UiElementType::Separator && element.bound.contains(px, py)) {
            popupHit = &element;
            break;
        }
    }
    ASSERT_NE(popupHit, nullptr);
    ASSERT_EQ(popupHit->id, item->id);

    // (d) click the resolved item -> popup closes, action dispatches.
    click(Ui::Render::UiElementType::MenuItem, popupHit->id);

    EXPECT_EQ(chrome.log,
              (std::vector<std::string> { "openPopup(" + std::to_string(parent->id) + ")",
                                          "closePopup",
                                          "emitAction(" + item->actionKey + "," + item->label + ")" }));
}

} // namespace PureGlUi
