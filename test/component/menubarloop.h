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

// The menu bar driven through the shell's own loop: the titles a click opens a list for,
// pointer input at one of them in the OS's queue, the passes that read it, and every event
// they read, for a failure to say

#include "testidkind.h"
#include "ui/config.h"
#include "ui/idkind.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/menu.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/event.h"
#include "ui/window/eventtype.h"
#include "ui/window/windowmanager.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace PureGlUi {

// Passes the loop runs: the ones a test scripts, then several with nothing new, for
// anything the shell still owes the pointer
constexpr int PASSES = 8;

// Every top menu a click opens a list for, in the bar's order
[[nodiscard]] inline std::vector<Ui::id_t> listMenuIdsOf(const Ui::Shell & shell)
{
    std::vector<Ui::id_t> menuIds;
    for (const Ui::Res::Type::menu_t & menu : shell.resManager().menus()) {
        if (menu.visible && menu.enabled && menu.actionKey.empty() && !menu.items.empty()) {
            menuIds.emplace_back(menu.id);
        }
    }
    return menuIds;
}

// The shell's own loop for PASSES passes, script called at the start of each with its number
inline void runPasses(Ui::Shell & shell, const std::function<void(int)> & script)
{
    int pass = 0;
    shell.setFrameTasks([&shell, &pass, &script] {
        script(pass);
        ++pass;
        if (pass >= PASSES) {
            shell.requestStop();
        }
    });
    shell.run();
}

// Pointer input at the middle of a menu's title, where the next poll finds it
template <typename post_t>
void postAt(Ui::Shell & shell, Ui::id_t menuId, post_t post)
{
    const Ui::Res::Type::bound_t & title = shell.windowManager().elementBound(menuId);
    post(shell.windowManager().mainWindow(),
         Ui::roundToInt(Ui::toPhys(title.x + (title.w / 2.0F))),
         Ui::roundToInt(Ui::toPhys(title.y + (title.h / 2.0F))));
}

// Every event the loop reads from here on, appended to trace in order: a resize's size, a
// pointer's px and whether a list's, so a failure says what the loop got and where
inline void traceEvents(Ui::Shell & shell, std::string & trace)
{
    trace                                     = "the loop read:";
    Ui::Window::WindowManager & windowManager = shell.windowManager();
    const auto                  traceAs = [&windowManager, &trace](Ui::Window::EventType type, std::string_view name) {
        windowManager.subscribe().add(Ui::idOf(Ui::IdKind::Event, type),
                                      Ui::idOf(TestIdKind::Subscriber, 0),
                                      [&windowManager, &trace, name] {
                                          const Ui::Window::Event & event = windowManager.currentEvent();
                                          trace += " ";
                                          trace += name;
                                          if (event.type == Ui::Window::EventType::Resize) {
                                              trace += " " + std::to_string(Ui::roundToInt(event.resize.width)) + "x"
                                                     + std::to_string(Ui::roundToInt(event.resize.height));
                                          } else if (event.type != Ui::Window::EventType::CloseRequested
                                                     && event.type != Ui::Window::EventType::ScaleChange
                                                     && event.type != Ui::Window::EventType::FocusLost) {
                                              trace += " " + std::to_string(event.mouse.x) + ","
                                                     + std::to_string(event.mouse.y)
                                                     + (event.isPopupEvent ? " list" : "");
                                          }
                                          trace += ";";
                                      });
    };
    traceAs(Ui::Window::EventType::CloseRequested, "close");
    traceAs(Ui::Window::EventType::Resize, "resize");
    traceAs(Ui::Window::EventType::ScaleChange, "scale");
    traceAs(Ui::Window::EventType::FocusLost, "focuslost");
    traceAs(Ui::Window::EventType::MouseMove, "move");
    traceAs(Ui::Window::EventType::MouseButtonPress, "press");
    traceAs(Ui::Window::EventType::MouseButtonRelease, "release");
    traceAs(Ui::Window::EventType::MouseLeave, "leave");
}

} // namespace PureGlUi
