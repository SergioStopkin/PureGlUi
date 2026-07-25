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
 * @file TestInitHooks.cpp
 * @brief Ui::init_hooks_t ordering + the action registry's completeness at
 *        construction. Guards the two invariants a host silently depends on.
 *
 * Why these: the whole point of the hooks is that the FRAMEWORK owns the order,
 * so a host cannot drift from it. Two things make that work and neither is
 * visible at a call site - loadDomainResources must run before the window exists
 * (WindowManager reads layout/theme/dock state from ResManager), and the built-in
 * actions must be bound before a host registers anything so an override wins by
 * overwrite in any order.
 *
 * runApp() itself is not tested here: it installs process signal handlers and
 * enters the run loop, so it cannot return inside a test. Its pieces are covered
 * instead - hooks here, session I/O in the unit suite's TestSession.
 *
 * Display-agnostic: initialize() creates a real window when one is available and
 * fails cleanly when it is not, so each case asserts against both outcomes.
 */

#include "ui/inithooks.h"
#include "ui/shell.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

// Record which hooks fired, in order.
Ui::init_hooks_t recordingHooks(std::vector<std::string> & log)
{
    return Ui::init_hooks_t {
        .loadDomainResources = [&log]() { log.emplace_back("loadDomainResources"); },
        .gateFeatures = [&log]() { log.emplace_back("gateFeatures"); },
        .afterWindowCreated = [&log]() { log.emplace_back("afterWindowCreated"); },
        .afterInit = [&log]() { log.emplace_back("afterInit"); },
        .afterReload = [&log]() { log.emplace_back("afterReload"); },
    };
}

// The built-in actions are bound by the constructor, not by wireEvents(), so the
// registry is already complete before a host touches it.
TEST(InitHooks, BuiltInActionsBoundAtConstruction)
{
    const Ui::Shell shell;

    EXPECT_TRUE(shell.actions().has("Reload"));
    EXPECT_TRUE(shell.actions().has("ExitApp"));
    EXPECT_TRUE(shell.actions().has("OpenFile"));
    EXPECT_TRUE(shell.actions().has("SwitchTheme"));
    EXPECT_TRUE(shell.actions().has("SwitchThemeMode"));
}

// A host overriding a built-in key wins, with no initialize() in between - the
// property that lets a host register whenever it likes.
TEST(InitHooks, HostOverrideOfABuiltInKeyWins)
{
    Ui::Shell shell;

    bool hostRan = false;
    shell.actions().on("Reload", [&hostRan](const std::string &) { hostRan = true; });
    shell.actions().dispatch("Reload");

    EXPECT_TRUE(hostRan);
}

// loadDomainResources runs before the window is created, so it is the one hook
// that fires even when window creation fails.
TEST(InitHooks, DomainResourcesLoadBeforeTheWindow)
{
    Ui::Shell                shell;
    std::vector<std::string> log;

    const bool initialized = shell.initialize(recordingHooks(log));

    ASSERT_FALSE(log.empty());
    EXPECT_EQ(log.front(), "loadDomainResources");

    if (initialized) {
        // Full spine: resources, window, host window-state, gate, then the tail.
        // afterWindowCreated precedes gateFeatures because the gate needs GL
        // capabilities but the renderer must not exist yet. The exact sequence
        // also pins that afterReload belongs to reloadChrome, never to init.
        EXPECT_EQ(
        log,
        (std::vector<std::string> { "loadDomainResources", "afterWindowCreated", "gateFeatures", "afterInit" }));
        shell.shutdown();
    } else {
        // No display: initWindow() failed right after the first hook, so nothing
        // window-dependent ran.
        EXPECT_EQ(log, (std::vector<std::string> { "loadDomainResources" }));
    }
}

// Every hook is optional: a standalone shell passes none and the spine still runs.
TEST(InitHooks, HooksAreOptional)
{
    Ui::Shell shell;

    const bool initialized = shell.initialize();
    if (initialized) {
        shell.shutdown();
    }
    SUCCEED(); // reaching here = no null-callback dereference
}

} // namespace
