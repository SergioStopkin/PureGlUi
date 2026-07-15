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
 * @brief Recording-seam prototype (T2 preview): test the chrome-lifecycle
 *        mapping - click -> intent -> window/chrome command - WITHOUT real GL
 *        windows, a FakeWindow, or a parameterized WindowManager.
 *
 * Shell::execute() maps each intent to a WindowManager/action command
 * (OpenPopup -> createMenuPopup, leaf click -> ClosePopup + EmitAction, etc.).
 * That mapping is the lifecycle logic we want under test. Rather than faking the
 * window layer to observe real popups appearing, we record at the *command*
 * boundary: an IChromeCommands seam the mapping drives, plus the real
 * Action::Registry (spy) and the real tab hooks.
 *
 * FAITHFULNESS: executeIntent() below is the mapping *extracted* from
 * Shell::execute(). The real adoption points Shell::execute() at this same
 * function (WindowManager supplying an IChromeCommands adapter: openPopup ->
 * createMenuPopup, closePopup -> destroyPopup, openDialog -> openDialog), so app
 * and test share ONE copy - no parallel path. Deferral/timing (subscribe().defer)
 * stays Shell-internal; this models the command *semantics* the lifecycle asserts.
 */

#include "ui/action/registry.h"
#include "ui/intent.h"
#include "ui/render/clickresult.h"
#include "ui/render/context.h"
#include "ui/res/resmanager.h"
#include "ui/type.h"

#include <functional>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

// The window/chrome-facing commands Shell::execute() issues. Production supplies
// these via a thin WindowManager adapter; the test records them.
class IChromeCommands {
public:
    virtual ~IChromeCommands()                      = default;
    virtual void openPopup(Ui::id_t menuId)         = 0;
    virtual void closePopup()                       = 0;
    virtual void openDialog(Ui::id_t itemId)        = 0;
    virtual void copyText(const std::string & text) = 0;
};

// Records the command sequence as readable strings for order-sensitive asserts.
class RecordingChrome final : public IChromeCommands {
public:
    std::vector<std::string> log;

    void openPopup(Ui::id_t menuId) override { log.emplace_back("openPopup(" + std::to_string(menuId) + ")"); }
    void closePopup() override { log.emplace_back("closePopup"); }
    void openDialog(Ui::id_t itemId) override { log.emplace_back("openDialog(" + std::to_string(itemId) + ")"); }
    void copyText(const std::string & text) override { log.emplace_back("copyText(" + text + ")"); }
};

// Host reactions Shell forwards (SwitchTab/CloseTab) - already std::function seams.
struct alignas(64) intent_hooks_t final {
    std::function<void(Ui::id_t)> onTabActivated;
    std::function<void(Ui::id_t)> onTabClosed;
};

// The intent -> command mapping extracted from Shell::execute(). Authored once;
// Shell delegates here in the real change. openMenuId is the popup-open state
// (Shell's m_openMenuId), threaded so ClosePopup only fires when one is open.
inline void executeIntent(const Ui::intent_t &   intent,
                          IChromeCommands &      chrome,
                          Ui::Action::Registry & actions,
                          const intent_hooks_t & hooks,
                          Ui::id_t &             openMenuId)
{
    switch (intent.kind) {
    case Ui::IntentKind::EmitAction: actions.dispatch(intent.actionKey, intent.arg); break;
    case Ui::IntentKind::OpenPopup:
        openMenuId = intent.id;
        chrome.openPopup(intent.id);
        break;
    case Ui::IntentKind::ClosePopup:
        if (openMenuId != Ui::INVALID_ID) {
            openMenuId = Ui::INVALID_ID;
            chrome.closePopup();
        }
        break;
    case Ui::IntentKind::OpenSubmenu: break; // hover-driven, no click path
    case Ui::IntentKind::OpenDialog: chrome.openDialog(intent.id); break;
    case Ui::IntentKind::SwitchTab:
        if (hooks.onTabActivated) {
            hooks.onTabActivated(intent.id);
        }
        break;
    case Ui::IntentKind::CloseTab:
        if (hooks.onTabClosed) {
            hooks.onTabClosed(intent.id);
        }
        break;
    case Ui::IntentKind::CopyText:
        if (!intent.arg.empty()) {
            chrome.copyText(intent.arg);
        }
        break;
    }
}

class ChromeLifecycleTest : public ::testing::Test {
protected:
    Ui::Res::ResManager  resManager { TEST_RES_DIR };
    Ui::Action::Registry actions;
    RecordingChrome      chrome;
    intent_hooks_t       hooks;
    Ui::id_t             openMenuId = Ui::INVALID_ID;

    void SetUp() override { resManager.loadAll(); }

    // Drive a click through the REAL Context, then the extracted mapping - the
    // same two stages dispatchClick() runs in production.
    void click(Ui::Render::UiElementType type, Ui::id_t id)
    {
        Ui::Render::Context        context(resManager);
        Ui::Render::click_result_t hit;
        hit.type                  = type;
        hit.id                    = id;
        const Ui::result_t result = context.mapClick(hit, openMenuId);
        for (const Ui::intent_t & intent : result.intents) {
            executeIntent(intent, chrome, actions, hooks, openMenuId);
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

    // First leaf item (under any top menu) matching a predicate.
    template <class Pred>
    [[nodiscard]] const Ui::Res::Type::menu_t * leafItem(Pred pred) const
    {
        for (const auto & menu : resManager.menus()) {
            for (const auto & item : menu.items) {
                if (!item.separator && item.items.empty() && pred(item)) {
                    return &item;
                }
            }
        }
        return nullptr;
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
    EXPECT_EQ(openMenuId, Ui::INVALID_ID);
}

// Clicking a leaf action item closes the popup, then dispatches its action with
// the item label as the arg (parameterized actions).
TEST_F(ChromeLifecycleTest, LeafActionClosesPopupAndDispatches)
{
    const Ui::Res::Type::menu_t * item = leafItem(
    [](const Ui::Res::Type::menu_t & i) { return !i.actionKey.empty() && i.dialog.title.empty(); });
    if (item == nullptr) {
        GTEST_SKIP() << "no leaf action item in res menus";
    }

    std::string firedKey;
    std::string firedArg;
    actions.on(item->actionKey, [&](const std::string & arg) {
        firedKey = item->actionKey;
        firedArg = arg;
    });

    openMenuId = 1; // a popup is open (its parent menu)
    click(Ui::Render::UiElementType::MenuItem, item->id);

    EXPECT_EQ(chrome.log, (std::vector<std::string> { "closePopup" }));
    EXPECT_EQ(firedKey, item->actionKey);
    EXPECT_EQ(firedArg, item->label);
}

// Clicking a leaf dialog item closes the popup, then opens its dialog.
TEST_F(ChromeLifecycleTest, LeafDialogClosesPopupAndOpensDialog)
{
    const Ui::Res::Type::menu_t * item = leafItem(
    [](const Ui::Res::Type::menu_t & i) { return !i.dialog.title.empty(); });
    if (item == nullptr) {
        GTEST_SKIP() << "no leaf dialog item in res menus";
    }

    openMenuId = 1;
    click(Ui::Render::UiElementType::MenuItem, item->id);

    EXPECT_EQ(chrome.log, (std::vector<std::string> { "closePopup", "openDialog(" + std::to_string(item->id) + ")" }));
}

// Clicking the status-bar text copies it to the clipboard.
TEST_F(ChromeLifecycleTest, StatusTextCopies)
{
    resManager.setStatusText("hello world");
    click(Ui::Render::UiElementType::Text, Ui::INVALID_ID);
    EXPECT_EQ(chrome.log, (std::vector<std::string> { "copyText(hello world)" }));
}

} // namespace PureGlUi
