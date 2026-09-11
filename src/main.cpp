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
// runApp() covers signals, session restore/save, init, run and the exit code. The
// banner stays here because only main() brackets the Shell's whole lifetime: its
// members log while being constructed, and its teardown runs when it goes out of
// scope - hence the explicit scope below, so "Bye!" really is the last line.
#include "ui/shell.h"

#include <exception>
#include <iostream>

int main()
{
    std::cout << "Hello!" << std::endl;

    int exitCode = 1;
    try {
        Ui::Shell shell;
        exitCode = shell.runApp();
    } catch (const std::exception & error) {
        // Leaving main is undefined behaviour, so the last frame that can still
        // report the failure is this one
        std::cerr << "[main] " << error.what() << std::endl;
    }

    std::cout << "Bye!" << std::endl;
    return exitCode;
}
