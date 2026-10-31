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

// What the OS itself says of a window - whether it is on screen, where its content stands,
// which pixels its shape shows - and the largest state it gives one, for a test to hold the
// framework's own answers against. Physical px, x right and y down from the primary
// screen's top-left

#include "common/bit.h"
#include "common/time.h"
#include "ui/interface/iwindow.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <chrono>

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
// NOLINTBEGIN(clang-diagnostic-import-preprocessor-directive-pedantic)
// The SDK's broken Icons.h kept out, as ui/window/platform/macoswindow.h does
#define __ICONS__
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
// NOLINTEND(clang-diagnostic-import-preprocessor-directive-pedantic)

#include <memory>
#elifdef HAVE_X11
#include "ui/window/platform/x11include.h"

#include <cstddef>
#include <cstdint>
#include <span>
#endif

namespace PureGlUi {

// How long the OS is given to act on what it was asked, and the most a change of state takes
constexpr std::chrono::milliseconds SETTLE { 100 };
constexpr std::chrono::milliseconds MOST_WAIT { 5000 };

#ifdef _WIN32

// The messages the OS queued for the window taken
inline void settle(const Ui::IWindow & /*window*/)
{
    MSG message {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    Common::Time::sleepFor(SETTLE);
}

inline bool isOnScreen(const Ui::IWindow & window) { return IsWindowVisible(window.nativeHandle()) != 0; }

inline Ui::Res::Type::bound_t nativeContentRectOf(const Ui::IWindow & window)
{
    RECT  client {};
    POINT origin {};
    GetClientRect(window.nativeHandle(), &client);
    ClientToScreen(window.nativeHandle(), &origin);
    return { .x = static_cast<Ui::fpx_t>(origin.x),
             .y = static_cast<Ui::fpx_t>(origin.y),
             .w = static_cast<Ui::fpx_t>(client.right),
             .h = static_cast<Ui::fpx_t>(client.bottom) };
}

// Through the window's region; one with none shows every pixel
inline bool isShownAt(const Ui::IWindow & window, int x, int y)
{
    HRGN       region  = CreateRectRgn(0, 0, 0, 0);
    const bool isShown = GetWindowRgn(window.nativeHandle(), region) == ERROR || PtInRegion(region, x, y) != 0;
    DeleteObject(region);
    return isShown;
}

#elifdef __APPLE__

// AppKit's event loop run for a moment, which is where it moves and animates windows
inline void settle(const Ui::IWindow & /*window*/)
{
    const Common::Time::point_t start = Common::Time::now();
    while (Common::Time::now() - start < SETTLE) {
        @autoreleasepool {
            NSEvent * event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES];
            if (event != nil) {
                [NSApp sendEvent:event];
            }
        }
    }
}

inline bool isOnScreen(const Ui::IWindow & window) { return [[window.nativeHandle() window] isVisible] != NO; }

// The view the window draws in, where AppKit has it: its bounds through its window to the
// screen, y flipped against the primary screen as the framework measures it
inline Ui::Res::Type::bound_t nativeContentRectOf(const Ui::IWindow & window)
{
    NSView *      view         = window.nativeHandle();
    NSWindow *    nsWindow     = [view window];
    const NSRect  onScreen     = [nsWindow convertRectToScreen:[view convertRect:[view bounds] toView:nil]];
    const CGFloat scale        = [nsWindow backingScaleFactor];
    const CGFloat screenHeight = [[NSScreen screens] firstObject].frame.size.height;
    return { .x = static_cast<Ui::fpx_t>(NSMinX(onScreen) * scale),
             .y = static_cast<Ui::fpx_t>((screenHeight - NSMaxY(onScreen)) * scale),
             .w = static_cast<Ui::fpx_t>(NSWidth(onScreen) * scale),
             .h = static_cast<Ui::fpx_t>(NSHeight(onScreen) * scale) };
}

// Through the mask the view's layer is cut by, at the pixel's centre: a layer measures up
// from its bottom-left, in points
inline bool isShownAt(const Ui::IWindow & window, int x, int y)
{
    NSView *  view = window.nativeHandle();
    CALayer * mask = [[view layer] mask];
    if (mask == nil) {
        return true;
    }
    const CGFloat scale  = [[view window] backingScaleFactor];
    const CGPoint inView = CGPointMake((x + 0.5) / scale, NSHeight([view bounds]) - ((y + 0.5) / scale));
    const CGPoint inMask = CGPointMake(inView.x - mask.frame.origin.x, inView.y - mask.frame.origin.y);
    if (![mask isKindOfClass:[CAShapeLayer class]]) {
        return CGRectContainsPoint([mask bounds], inMask);
    }
    const id       maskObject = mask;
    CAShapeLayer * shape      = maskObject;
    return CGPathContainsPoint([shape path], nullptr, inMask, false);
}

#elifdef HAVE_X11

// The requests answered, then a moment for the window manager's own
inline void settle(const Ui::IWindow & window)
{
    XSync(window.nativeDisplay(), Ui::Window::Platform::X11::False);
    Common::Time::sleepFor(SETTLE);
}

// Mapped, and every window it stands in too
inline bool isOnScreen(const Ui::IWindow & window)
{
    XWindowAttributes attributes {};
    XGetWindowAttributes(window.nativeDisplay(), window.nativeHandle(), &attributes);
    return attributes.map_state == IsViewable;
}

inline Ui::Res::Type::bound_t nativeContentRectOf(const Ui::IWindow & window)
{
    Display *         display = window.nativeDisplay();
    XWindowAttributes attributes {};
    XGetWindowAttributes(display, window.nativeHandle(), &attributes);
    ::Window child {};
    int      x {};
    int      y {};
    XTranslateCoordinates(display, window.nativeHandle(), DefaultRootWindow(display), 0, 0, &x, &y, &child);
    return { .x = static_cast<Ui::fpx_t>(x),
             .y = static_cast<Ui::fpx_t>(y),
             .w = static_cast<Ui::fpx_t>(attributes.width),
             .h = static_cast<Ui::fpx_t>(attributes.height) };
}

// Through the bounding shape, which is the window's own rectangle when none is set
inline bool isShownAt(const Ui::IWindow & window, int x, int y)
{
    int          count {};
    int          ordering {};
    XRectangle * rectangles =
    XShapeGetRectangles(window.nativeDisplay(), window.nativeHandle(), ShapeBounding, &count, &ordering);
    bool isShown = false;
    for (const XRectangle & rectangle : std::span(rectangles, static_cast<std::size_t>(count))) {
        const Ui::Res::Type::bound_t shown { .x = static_cast<Ui::fpx_t>(rectangle.x),
                                             .y = static_cast<Ui::fpx_t>(rectangle.y),
                                             .w = static_cast<Ui::fpx_t>(rectangle.width),
                                             .h = static_cast<Ui::fpx_t>(rectangle.height) };
        isShown = isShown || shown.contains(x, y);
    }
    if (rectangles != nullptr) {
        XFree(rectangles);
    }
    return isShown;
}

#endif

// Asks the OS until isReached holds, false past MOST_WAIT; once it does, a moment more for
// what the OS does after it says so
template <typename predicate_t>
[[nodiscard]] bool waitUntil(const Ui::IWindow & window, predicate_t isReached)
{
    const Common::Time::point_t start = Common::Time::now();
    while (!isReached()) {
        if (Common::Time::now() - start > MOST_WAIT) {
            return false;
        }
        settle(window);
    }
    settle(window);
    return true;
}

// The largest state the OS gives a window, or back out of it: Win32's maximized, macOS's
// fullscreen, an X11 window manager's. False when the OS did not get it there
#ifdef _WIN32

[[nodiscard]] inline bool setEnlarged(const Ui::IWindow & window, bool isEnlarged)
{
    ShowWindow(window.nativeHandle(), isEnlarged ? SW_MAXIMIZE : SW_RESTORE);
    return waitUntil(window, [&window, isEnlarged] { return (IsZoomed(window.nativeHandle()) != 0) == isEnlarged; });
}

#elifdef __APPLE__

// Done at AppKit's did-enter or did-exit notification, the end of the animation: the
// window's style and frame change as it starts, and a window torn down before it ends
// leaves AppKit starting no other
[[nodiscard]] inline bool setEnlarged(const Ui::IWindow & window, bool isEnlarged)
{
    NSWindow * nsWindow = [window.nativeHandle() window];
    if ((Common::Bit::And([nsWindow styleMask], NSWindowStyleMaskFullScreen) != 0) == isEnlarged) {
        return true;
    }
    const auto               isDone   = std::make_shared<bool>(false);
    NSNotificationCenter *   center   = [NSNotificationCenter defaultCenter];
    const NSNotificationName name     = isEnlarged ? NSWindowDidEnterFullScreenNotification
                                                   : NSWindowDidExitFullScreenNotification;
    const id                 observer = [center addObserverForName:name
                                                            object:nsWindow
                                                             queue:nil
                                                        usingBlock:^(NSNotification * /*note*/) { *isDone = true; }];
    [nsWindow toggleFullScreen:nil];
    const bool isReached = waitUntil(window, [isDone] { return *isDone; });
    [center removeObserver:observer];
    return isReached;
}

#elifdef HAVE_X11

// Fullscreen as an EWMH window manager gives it, the window its screen's size; with no
// such manager running nothing would answer, so nothing is asked
[[nodiscard]] inline bool setEnlarged(const Ui::IWindow & window, bool isEnlarged)
{
    Display * display = window.nativeDisplay();
    if (XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", Ui::Window::Platform::X11::True) == 0) {
        return false;
    }
    XEvent event {};
    event.xclient.type         = ClientMessage;
    event.xclient.window       = window.nativeHandle();
    event.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", Ui::Window::Platform::X11::False);
    event.xclient.format       = 32;
    // _NET_WM_STATE_ADD or _REMOVE, the state, no second one, a normal application asking
    event.xclient.data.l[0] = isEnlarged ? 1 : 0;
    // NOLINTNEXTLINE(google-runtime-int)
    event.xclient.data.l[1] = static_cast<long>(
    XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", Ui::Window::Platform::X11::False));
    event.xclient.data.l[3] = 1;
    XSendEvent(display,
               DefaultRootWindow(display),
               Ui::Window::Platform::X11::False,
               static_cast<int64_t>(Common::Bit::Or(SubstructureRedirectMask, SubstructureNotifyMask)),
               &event);
    XFlush(display);

    return waitUntil(window, [&window, isEnlarged] {
        XWindowAttributes attributes {};
        XGetWindowAttributes(window.nativeDisplay(), window.nativeHandle(), &attributes);
        const bool isFullscreen = attributes.width == WidthOfScreen(attributes.screen)
                               && attributes.height == HeightOfScreen(attributes.screen);
        return isFullscreen == isEnlarged;
    });
}

#endif

} // namespace PureGlUi
