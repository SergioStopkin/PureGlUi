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

// Pointer input put in the OS's own event queue, as the OS puts it, for a window's event
// loop to read at its next poll - so a test drives the loop the app runs, batches and all,
// rather than the handlers behind it. Physical px from the window's content corner; a
// press and a release are the left button's

#include "ui/interface/iwindow.h"

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
// NOLINTBEGIN(clang-diagnostic-import-preprocessor-directive-pedantic)
// The SDK's broken Icons.h kept out, as ui/window/platform/macoswindow.h does
#define __ICONS__
#import <Cocoa/Cocoa.h>
// NOLINTEND(clang-diagnostic-import-preprocessor-directive-pedantic)
#elifdef HAVE_X11
#include "ui/window/platform/x11include.h"
#endif

namespace PureGlUi {

#ifdef _WIN32

inline void postPointerMove(const Ui::IWindow & window, int x, int y)
{
    PostMessageW(window.nativeHandle(), WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
}

inline void postPointerPress(const Ui::IWindow & window, int x, int y)
{
    PostMessageW(window.nativeHandle(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
}

inline void postPointerRelease(const Ui::IWindow & window, int x, int y)
{
    PostMessageW(window.nativeHandle(), WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}

#elifdef __APPLE__

// At locationInWindow, where AppKit reads one: points up from the content's bottom-left.
// A press or a release is a click's, one at a time
inline void postMouseEvent(const Ui::IWindow & window, NSEventType type, int x, int y)
{
    NSWindow *    nsWindow = [window.nativeHandle() window];
    const CGFloat scale    = [nsWindow backingScaleFactor];
    const NSPoint location = NSMakePoint(x / scale, NSHeight([[nsWindow contentView] bounds]) - (y / scale));
    const bool    isButton = type != NSEventTypeMouseMoved;
    NSEvent *     event    = [NSEvent mouseEventWithType:type
                                                location:location
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:[nsWindow windowNumber]
                                                 context:nil
                                             eventNumber:0
                                              clickCount:isButton ? 1 : 0
                                                pressure:type == NSEventTypeLeftMouseDown ? 1.0F : 0.0F];
    [NSApp postEvent:event atStart:NO];
}

inline void postPointerMove(const Ui::IWindow & window, int x, int y)
{
    postMouseEvent(window, NSEventTypeMouseMoved, x, y);
}

inline void postPointerPress(const Ui::IWindow & window, int x, int y)
{
    postMouseEvent(window, NSEventTypeLeftMouseDown, x, y);
}

inline void postPointerRelease(const Ui::IWindow & window, int x, int y)
{
    postMouseEvent(window, NSEventTypeLeftMouseUp, x, y);
}

#elifdef HAVE_X11

// Sent to the window, then synced, so it waits in the queue the next poll reads
inline void postPointerMove(const Ui::IWindow & window, int x, int y)
{
    Display * display = window.nativeDisplay();
    XEvent    event {};
    event.xmotion.type        = MotionNotify;
    event.xmotion.display     = display;
    event.xmotion.window      = window.nativeHandle();
    event.xmotion.root        = DefaultRootWindow(display);
    event.xmotion.x           = x;
    event.xmotion.y           = y;
    event.xmotion.same_screen = Ui::Window::Platform::X11::True;
    XSendEvent(display, window.nativeHandle(), Ui::Window::Platform::X11::False, PointerMotionMask, &event);
    XSync(display, Ui::Window::Platform::X11::False);
}

// The first button going down (ButtonPress) or up (ButtonRelease, with it in the state)
inline void postPointerButton(const Ui::IWindow & window, int type, int x, int y)
{
    Display * display = window.nativeDisplay();
    XEvent    event {};
    event.xbutton.type        = type;
    event.xbutton.display     = display;
    event.xbutton.window      = window.nativeHandle();
    event.xbutton.root        = DefaultRootWindow(display);
    event.xbutton.x           = x;
    event.xbutton.y           = y;
    event.xbutton.button      = Button1;
    event.xbutton.state       = type == ButtonRelease ? Button1Mask : 0;
    event.xbutton.same_screen = Ui::Window::Platform::X11::True;
    XSendEvent(display,
               window.nativeHandle(),
               Ui::Window::Platform::X11::False,
               type == ButtonPress ? ButtonPressMask : ButtonReleaseMask,
               &event);
    XSync(display, Ui::Window::Platform::X11::False);
}

inline void postPointerPress(const Ui::IWindow & window, int x, int y) { postPointerButton(window, ButtonPress, x, y); }

inline void postPointerRelease(const Ui::IWindow & window, int x, int y)
{
    postPointerButton(window, ButtonRelease, x, y);
}

#endif

} // namespace PureGlUi
