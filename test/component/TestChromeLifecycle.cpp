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
 * double stands in for Shell's IChromeCommands impl (which forwards to
 * WindowManager + the action registry + host tab hooks). So there is one copy of
 * the routing, shared by app and test - not a parallel reimplementation.
 */

#include "ui/intent.h"
#include "ui/interface/ichromecommands.h"
#include "ui/render/clickresult.h"
#include "ui/render/context.h"
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

// Records the command sequence Ui::routeIntent issues, as readable strings for
// order-sensitive asserts. Tracks the open-menu id so isPopupOpen() gates the
// ClosePopup path exactly as Shell's m_openMenuId does.
class RecordingChrome final : public Ui::IChromeCommands {
public:
    std::vector<std::string> log;
    Ui::id_t                 openMenuId = Ui::INVALID_ID;

    void emitAction(const std::string & actionKey, const std::string & arg) override
    {
        log.emplace_back("emitAction(" + actionKey + "," + arg + ")");
    }
    [[nodiscard]] bool isPopupOpen() const override { return openMenuId != Ui::INVALID_ID; }
    void               openPopup(Ui::id_t menuId) override
    {
        openMenuId = menuId;
        log.emplace_back("openPopup(" + std::to_string(menuId) + ")");
    }
    void closePopup() override
    {
        openMenuId = Ui::INVALID_ID;
        log.emplace_back("closePopup");
    }
    void openDialog(Ui::id_t itemId) override { log.emplace_back("openDialog(" + std::to_string(itemId) + ")"); }
    void switchTab(Ui::id_t tabId) override { log.emplace_back("switchTab(" + std::to_string(tabId) + ")"); }
    void closeTab(Ui::id_t tabId) override { log.emplace_back("closeTab(" + std::to_string(tabId) + ")"); }
    void copyText(const std::string & text) override { log.emplace_back("copyText(" + text + ")"); }
};

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

    // First top menu that opens a dropdown (no directly-bound action).
    [[nodiscard]] const Ui::Res::Type::menu_t * popupMenu() const
    {
        for (const auto & menu : resManager.menus()) {
            if (resManager.actionKeyFor(menu.id).empty()) {
                return &menu;
            }
        }
        return nullptr;
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
    const Ui::Res::Type::menu_t * menu = popupMenu();
    ASSERT_NE(menu, nullptr);

    click(Ui::Render::UiElementType::MenuButton, menu->id);
    click(Ui::Render::UiElementType::MenuButton, menu->id);

    EXPECT_EQ(chrome.log, (std::vector<std::string> { "openPopup(" + std::to_string(menu->id) + ")", "closePopup" }));
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

} // namespace PureGlUi
