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

// Pointer input, a Ctrl going up, and the request the frame's close button makes, put in the OS's own event
// queue as the OS puts them, for a window's event loop to read at its next poll - so a test
// drives the loop the app runs, batches and all, rather than the handlers behind it.
// Physical px from the window's content corner; a press and a release are the left button's

#include "ui/interface/iwindow.h"

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
// NOLINTBEGIN(clang-diagnostic-import-preprocessor-directive-pedantic)
// The SDK's broken Icons.h kept out, as ui/window/platform/macoswindow.h does
#define __ICONS__
#import <Cocoa/Cocoa.h>
// NOLINTEND(clang-diagnostic-import-preprocessor-directive-pedantic)
#include "testconst.h"
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

inline void postCloseRequest(const Ui::IWindow & window) { PostMessageW(window.nativeHandle(), WM_CLOSE, 0, 0); }

// The left Ctrl going up: WM_KEYUP for VK_CONTROL, its lParam a release's (one, not extended,
// down before, going up)
inline void postControlRelease(const Ui::IWindow & window)
{
    PostMessageW(window.nativeHandle(), WM_KEYUP, VK_CONTROL, MAKELPARAM(1, 0xC000));
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

// A move off every window of the app, as AppKit can send one: no window, and its location on
// the screen rather than in one
inline void postPointerMoveOffWindows()
{
    NSEvent * event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                         location:NSMakePoint(1.0, 1.0)
                                    modifierFlags:0
                                        timestamp:0
                                     windowNumber:0
                                          context:nil
                                      eventNumber:0
                                       clickCount:0
                                         pressure:0.0F];
    [NSApp postEvent:event atStart:NO];
}

// As a click on its close button: AppKit closes the window at once, its will-close
// notification sent before this returns
inline void postCloseRequest(const Ui::IWindow & window) { [[window.nativeHandle() window] performClose:nil]; }

// A click on the app menu's Quit, found by its key as a user knows it. False where AppKit has
// it greyed, which a click does nothing on
[[nodiscard]] inline bool clickQuit()
{
    NSMenu * appMenu = [[[NSApp mainMenu] itemAtIndex:0] submenu];
    [appMenu update];
    for (NSMenuItem * item in [appMenu itemArray]) {
        if ([[item keyEquivalent] isEqualToString:@"q"] && [item isEnabled]) {
            [appMenu performActionForItemAtIndex:[appMenu indexOfItem:item]];
            return true;
        }
    }
    return false;
}

// Q pressed as the OS reports it: to the key window, or to no window with none key
inline void postQ(NSEventModifierFlags modifiers)
{
    NSEvent * event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                       location:NSZeroPoint
                                  modifierFlags:modifiers
                                      timestamp:0
                                   windowNumber:[[NSApp keyWindow] windowNumber]
                                        context:nil
                                     characters:@"q"
                    charactersIgnoringModifiers:@"q"
                                      isARepeat:NO
                                        keyCode:MAC_KEY_Q];
    [NSApp postEvent:event atStart:NO];
}

// The left Ctrl going up as AppKit reports a modifier alone: a change of the flags, Control
// no longer among them, never a key-up
inline void postControlRelease(const Ui::IWindow & /*window*/)
{
    NSEvent * event = [NSEvent keyEventWithType:NSEventTypeFlagsChanged
                                       location:NSZeroPoint
                                  modifierFlags:0
                                      timestamp:0
                                   windowNumber:[[NSApp keyWindow] windowNumber]
                                        context:nil
                                     characters:@""
                    charactersIgnoringModifiers:@""
                                      isARepeat:NO
                                        keyCode:MAC_KEY_CONTROL];
    [NSApp postEvent:event atStart:NO];
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

// What a window manager sends for the frame's close button: WM_PROTOCOLS carrying
// WM_DELETE_WINDOW, synced as the pointer's input is
inline void postCloseRequest(const Ui::IWindow & window)
{
    Display * display = window.nativeDisplay();
    XEvent    event {};
    event.xclient.type         = ClientMessage;
    event.xclient.display      = display;
    event.xclient.window       = window.nativeHandle();
    event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", Ui::Window::Platform::X11::False);
    event.xclient.format       = 32;
    // NOLINTNEXTLINE(google-runtime-int)
    event.xclient.data.l[0] = static_cast<long>(
    XInternAtom(display, "WM_DELETE_WINDOW", Ui::Window::Platform::X11::False));
    XSendEvent(display, window.nativeHandle(), Ui::Window::Platform::X11::False, NoEventMask, &event);
    XSync(display, Ui::Window::Platform::X11::False);
}

// The left Ctrl going up, Control still in the state as X reports the key that held it.
// Sent as the X server sends one: only to a window that selected KeyReleaseMask
inline void postControlRelease(const Ui::IWindow & window)
{
    Display * display = window.nativeDisplay();
    XEvent    event {};
    event.xkey.type        = Ui::Window::Platform::X11::KeyRelease;
    event.xkey.display     = display;
    event.xkey.window      = window.nativeHandle();
    event.xkey.root        = DefaultRootWindow(display);
    event.xkey.keycode     = XKeysymToKeycode(display, XK_Control_L);
    event.xkey.state       = ControlMask;
    event.xkey.same_screen = Ui::Window::Platform::X11::True;
    XSendEvent(display, window.nativeHandle(), Ui::Window::Platform::X11::False, KeyReleaseMask, &event);
    XSync(display, Ui::Window::Platform::X11::False);
}

#endif

} // namespace PureGlUi
