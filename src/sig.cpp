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

#include "sig.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>

namespace {
std::function<void()> g_stop; // single internal definition
} // namespace

void g_app_init(std::function<void()> stopCallback) noexcept { g_stop = std::move(stopCallback); }

void on_signal(int sig) noexcept
{
    static std::atomic<bool> s_shutdownRequested { false };

    // Note: printing in a signal handler is not async-signal-safe; acceptable for CI/dev.
    std::cout << "\n\nSignal " << signal_name(sig) << "(" << sig << ") caught. Shutdown requested." << std::endl;

    if (s_shutdownRequested.exchange(true)) {
        std::cout << "Forced exit." << std::endl;
        std::_Exit(1);
    }

    // Only set the running flag to false - let the main loop exit gracefully
    // and call shutdown() from the main thread. Calling shutdown() directly
    // from a signal handler can interrupt system calls (X11, EGL) mid-flight,
    // causing hangs or crashes.
    if (g_stop) {
        g_stop();
    }
}

std::string_view signal_name(int sig) noexcept
{
    switch (sig) {
    case SIGINT: return "SIGINT";
    case SIGTERM: return "SIGTERM";
#if defined(SIGHUP)
    case SIGHUP: return "SIGHUP";
#endif
#if defined(SIGQUIT)
    case SIGQUIT: return "SIGQUIT";
#endif
#if defined(SIGABRT)
    case SIGABRT: return "SIGABRT";
#endif
#if defined(SIGSEGV)
    case SIGSEGV: return "SIGSEGV";
#endif
    default: return "UNKNOWN";
    }
}
