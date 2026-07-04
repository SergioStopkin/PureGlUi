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

// Demo entry point: runs the UI framework standalone. It composes Ui::Shell -
// no OCCT, no domain content - proving include/ui/ is a complete, runnable
// framework. The window opens, chrome renders, menus/dialogs/theme-switch/
// shortcuts work; the central content region stays the theme background (no
// content surfaces are registered).
//
// The one host-side touch is session persistence: this demo restores window
// geometry + theme (+ dock state) on launch and saves them on exit. The format
// is the framework's concern - serializeSession()/deserializeSession() hand back
// an opaque blob - so this file only does plain file I/O (no JSON dependency).
#include "sig.h"
#include "ui/shell.h"

#include <csignal>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string sessionPath(const Ui::Res::ResManager & res) { return res.sessionDir() + "/" + res.sessionFile(); }

// Restore persisted state before the window is created (geometry is picked up at
// window creation). The blob is opaque here; ResManager owns the format.
void restoreSession(Ui::Res::ResManager & res)
{
    const std::ifstream file(sessionPath(res));
    if (!file) {
        return; // no prior session
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    res.deserializeSession(buffer.str());
}

// Write the current persisted state to <sessionDir>/session.json.
void saveSession(const Ui::Res::ResManager & res)
{
    std::error_code ec;
    std::filesystem::create_directories(res.sessionDir(), ec);
    std::ofstream file(sessionPath(res));
    if (file) {
        file << res.serializeSession();
    }
}

} // namespace

int main()
{
    try {
        Ui::Shell shell;

        // Host-agnostic signal handling: stop the shell loop on SIGINT/SIGTERM.
        g_app_init([&shell]() { shell.requestStop(); });
        (void)std::signal(SIGINT, on_signal);
        (void)std::signal(SIGTERM, on_signal);

        if (!shell.initialize([&shell]() { restoreSession(shell.resManager()); })) {
            std::cerr << "Failed to initialize Shell" << std::endl;
            return 1;
        }

        shell.run();
        saveSession(shell.resManager()); // persist on clean exit (incl. SIGINT/SIGTERM)
        shell.shutdown();
        return 0;

    } catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
