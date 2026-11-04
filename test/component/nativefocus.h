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

// The focus as each OS moves it: away from the app or back to it, and onto a window of its own

#include "ui/interface/iwindow.h"

#ifdef HAVE_WAYLAND
#include "ui/window/nativewindow.h"

#include <wayland-client.h>
#else
#include "nativegeometry.h"
#endif

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
// NOLINTBEGIN(clang-diagnostic-import-preprocessor-directive-pedantic)
// The SDK's broken Icons.h kept out, as ui/window/platform/macoswindow.h does
#define __ICONS__
#import <Cocoa/Cocoa.h>
// NOLINTEND(clang-diagnostic-import-preprocessor-directive-pedantic)
#elif !defined(HAVE_WAYLAND)
#include "common/bit.h"
#include "ui/window/platform/x11include.h"
#endif

namespace PureGlUi {

// The app made inactive, as a click on another app's window makes it, or active again.
// False when the OS kept it as it was
#ifdef HAVE_WAYLAND

// The compositor's alone to give or take, so what it says once more
[[nodiscard]] inline bool setActive(const Ui::Window::NativeWindow & window, bool isActive)
{
    wl_display_roundtrip(window.nativeDisplay());
    return window.isActivated() == isActive;
}

#elifdef _WIN32

// The desktop's shell window brought to the front, or this one back with its input queue
// joined to the front one's, the rights a process no longer in front needs to take it
[[nodiscard]] inline bool setActive(const Ui::IWindow & window, bool isActive)
{
    if (isActive) {
        const DWORD frontThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        const DWORD thisThread  = GetCurrentThreadId();
        AttachThreadInput(thisThread, frontThread, TRUE);
        SetForegroundWindow(window.nativeHandle());
        AttachThreadInput(thisThread, frontThread, FALSE);
    } else if (GetShellWindow() == nullptr || SetForegroundWindow(GetShellWindow()) == 0) {
        return false;
    }
    return waitUntil(window,
                     [&window, isActive] { return (GetForegroundWindow() == window.nativeHandle()) == isActive; });
}

#elifdef __APPLE__

// AppKit does it within its own loop, so it is waited out
[[nodiscard]] inline bool setActive(const Ui::IWindow & window, bool isActive)
{
    if (isActive) {
        [NSApp activateIgnoringOtherApps:YES];
    } else {
        [NSApp deactivate];
    }
    return waitUntil(window, [isActive] { return ([NSApp isActive] != NO) == isActive; });
}

// Whether AppKit has a window key, waited out
[[nodiscard]] inline bool hasKeyWindow(const Ui::IWindow & window)
{
    return waitUntil(window, [] { return [NSApp keyWindow] != nil; });
}

#else

// The input focus put on the root window, which no app owns, or back on this one
[[nodiscard]] inline bool setActive(const Ui::IWindow & window, bool isActive)
{
    // As focusOn: the focus put on a window not yet viewable is a fatal BadMatch
    if (isActive && !waitUntil(window, [&window] { return isOnScreen(window); })) {
        return false;
    }
    Display * display = window.nativeDisplay();
    XSetInputFocus(display, isActive ? window.nativeHandle() : DefaultRootWindow(display), RevertToParent, CurrentTime);
    return waitUntil(window, [&window, isActive] {
        ::Window focus {};
        int      revertTo {};
        XGetInputFocus(window.nativeDisplay(), &focus, &revertTo);
        return (focus == window.nativeHandle()) == isActive;
    });
}

#endif

// Lists made top-level windows where they would be children of the main window, whose focus
// a child shares: only on X11 with no compositor
#if defined(_WIN32) || defined(__APPLE__) || defined(HAVE_WAYLAND)

inline void makeListsTopLevel(const Ui::IWindow & /*window*/) { }

#else

// The main window claims to be a compositor until it goes, which makes a list a window of
// root, as on a composited desktop. Never a real one's claim: taking that can end it
inline void makeListsTopLevel(const Ui::IWindow & window)
{
    Display *  display    = window.nativeDisplay();
    const Atom compositor = XInternAtom(display, "_NET_WM_CM_S0", Ui::Window::Platform::X11::False);
    if (XGetSelectionOwner(display, compositor) == Ui::Window::Platform::X11::None) {
        XSetSelectionOwner(display, compositor, window.nativeHandle(), CurrentTime);
        XSync(display, Ui::Window::Platform::X11::False);
    }
}

#endif

// The focus given to a list as its OS gives it. False where the OS keeps it from the list
#ifdef HAVE_WAYLAND

// No client can move it: whatever the compositor does as a list maps, answered
[[nodiscard]] inline bool giveFocus(const Ui::IWindow & window)
{
    wl_display_roundtrip(window.nativeDisplay());
    return true;
}

#elifdef _WIN32

// What a click on it does: a list is shown without activating, and activates on one
[[nodiscard]] inline bool giveFocus(const Ui::IWindow & window)
{
    SetForegroundWindow(window.nativeHandle());
    return waitUntil(window, [&window] { return GetForegroundWindow() == window.nativeHandle(); });
}

#elifdef __APPLE__

// What a click on it asks of AppKit, which a borderless window refuses by never becoming key
[[nodiscard]] inline bool giveFocus(const Ui::IWindow & window)
{
    [[window.nativeHandle() window] makeKeyAndOrderFront:nil];
    settle(window);
    return true;
}

#else

// Whether the window refuses the focus in WM_HINTS (ICCCM 4.1.7)
[[nodiscard]] inline bool refusesFocus(const Ui::IWindow & window)
{
    XWMHints * hints     = XGetWMHints(window.nativeDisplay(), window.nativeHandle());
    const bool isRefused = hints != nullptr && Common::Bit::And(hints->flags, InputHint) != 0
                        && hints->input == Ui::Window::Platform::X11::False;
    if (hints != nullptr) {
        XFree(hints);
    }
    return isRefused;
}

// The focus put on the window whatever it asks in WM_HINTS. The map waited for, since the focus
// put on a window not yet viewable is a BadMatch, fatal with no error handler. False where it
// never maps, or the focus does not stay there
[[nodiscard]] inline bool focusOn(const Ui::IWindow & window)
{
    if (!waitUntil(window, [&window] { return isOnScreen(window); })) {
        return false;
    }
    XSetInputFocus(window.nativeDisplay(), window.nativeHandle(), RevertToParent, CurrentTime);
    return waitUntil(window, [&window] {
        ::Window focus {};
        int      revertTo {};
        XGetInputFocus(window.nativeDisplay(), &focus, &revertTo);
        return focus == window.nativeHandle();
    });
}

// As a window manager gives it as a window maps, none running on CI: unless the window refuses it
[[nodiscard]] inline bool giveFocus(const Ui::IWindow & window) { return refusesFocus(window) || focusOn(window); }

#endif

} // namespace PureGlUi
