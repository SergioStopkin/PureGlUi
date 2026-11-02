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
// which pixels its shape shows - and what it does to one: its largest state, a move or a
// resize as its own frame makes them, and the app's activation, for a test to hold
// the framework's own answers against. Physical px, x right and y down from the primary
// screen's top-left

#include "common/bit.h"
#include "common/time.h"
#include "ui/config.h"
#include "ui/interface/iwindow.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <chrono>
#include <string>

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
// NOLINTBEGIN(clang-diagnostic-import-preprocessor-directive-pedantic)
// The SDK's broken Icons.h kept out, as ui/window/platform/macoswindow.h does
#define __ICONS__
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
// NOLINTEND(clang-diagnostic-import-preprocessor-directive-pedantic)

#include <cmath>
#include <memory>
#elifdef HAVE_X11
#include "common/bytes.h"
#include "common/cstr.h"
#include "ui/window/platform/x11include.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#endif

namespace PureGlUi {

// How long the OS is given to act on what it was asked, and the most a change of state takes
constexpr std::chrono::milliseconds SETTLE { 100 };
constexpr std::chrono::milliseconds MOST_WAIT { 5000 };

// A rect as a failure says it
[[nodiscard]] inline std::string textOf(const Ui::Res::Type::bound_t & bound)
{
    return std::to_string(Ui::roundToInt(bound.x)) + "," + std::to_string(Ui::roundToInt(bound.y)) + " "
         + std::to_string(Ui::roundToInt(bound.w)) + "x" + std::to_string(Ui::roundToInt(bound.h));
}

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

// What the OS says of the window now, for a failure to say
[[nodiscard]] inline std::string placementOf(const Ui::IWindow & window)
{
    return "content " + textOf(nativeContentRectOf(window));
}

// Through the window's region; one with none shows every pixel
inline bool isShownAt(const Ui::IWindow & window, int x, int y)
{
    HRGN       region  = CreateRectRgn(0, 0, 0, 0);
    const bool isShown = GetWindowRgn(window.nativeHandle(), region) == ERROR || PtInRegion(region, x, y) != 0;
    DeleteObject(region);
    return isShown;
}

// Moved and sized by delta's px, frame and all, at once: the window's own messages for it
// are sent before this returns
inline void moveResizeBy(const Ui::IWindow & window, const Ui::Res::Type::bound_t & delta)
{
    RECT frame {};
    GetWindowRect(window.nativeHandle(), &frame);
    SetWindowPos(window.nativeHandle(),
                 nullptr,
                 frame.left + Ui::roundToInt(delta.x),
                 frame.top + Ui::roundToInt(delta.y),
                 frame.right - frame.left + Ui::roundToInt(delta.w),
                 frame.bottom - frame.top + Ui::roundToInt(delta.h),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

#elifdef __APPLE__

// AppKit's event loop run once, for up to 10 ms
inline void runAppKitOnce()
{
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

// An AppKit rect, points up from the primary screen's bottom, as CoreGraphics has one: y
// down from its top
inline Ui::Res::Type::bound_t topDownOf(NSRect rect)
{
    const CGFloat screenHeight = [[NSScreen screens] firstObject].frame.size.height;
    return { .x = static_cast<Ui::fpx_t>(NSMinX(rect)),
             .y = static_cast<Ui::fpx_t>(screenHeight - NSMaxY(rect)),
             .w = static_cast<Ui::fpx_t>(NSWidth(rect)),
             .h = static_cast<Ui::fpx_t>(NSHeight(rect)) };
}

// Where the window server has the window, top-down in points; no size where it shows it nowhere
inline Ui::Res::Type::bound_t serverBoundsOf(NSWindow * nsWindow)
{
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow,
                                                 static_cast<CGWindowID>([nsWindow windowNumber]));
    if (list == nullptr) {
        return {};
    }
    CGRect shown = CGRectNull;
    if (CFArrayGetCount(list) > 0) {
        const auto info   = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(list, 0));
        const auto bounds = static_cast<CFDictionaryRef>(CFDictionaryGetValue(info, kCGWindowBounds));
        if (bounds != nullptr) {
            CGRectMakeWithDictionaryRepresentation(bounds, &shown);
        }
    }
    CFRelease(list);
    if (CGRectIsNull(shown)) {
        return {};
    }
    return { .x = static_cast<Ui::fpx_t>(CGRectGetMinX(shown)),
             .y = static_cast<Ui::fpx_t>(CGRectGetMinY(shown)),
             .w = static_cast<Ui::fpx_t>(CGRectGetWidth(shown)),
             .h = static_cast<Ui::fpx_t>(CGRectGetHeight(shown)) };
}

// The window server has the window where AppKit has it, within a point, or shows it nowhere
inline bool isPlaced(NSWindow * nsWindow)
{
    const Ui::Res::Type::bound_t server = serverBoundsOf(nsWindow);
    const Ui::Res::Type::bound_t frame  = topDownOf([nsWindow frame]);
    return server.w == 0.0F
        || (std::abs(server.x - frame.x) <= 1.0F && std::abs(server.y - frame.y) <= 1.0F
            && std::abs(server.w - frame.w) <= 1.0F && std::abs(server.h - frame.h) <= 1.0F);
}

// AppKit's event loop run until the window server has the window where AppKit put it, then
// a moment more, which is where AppKit moves and animates windows: a press posted before is
// placed against the frame the window had before its latest change, by as much as it moved
inline void settle(const Ui::IWindow & window)
{
    NSWindow *                  nsWindow = [window.nativeHandle() window];
    const Common::Time::point_t start    = Common::Time::now();
    while (!isPlaced(nsWindow) && Common::Time::now() - start < MOST_WAIT) {
        runAppKitOnce();
    }
    const Common::Time::point_t placed = Common::Time::now();
    while (Common::Time::now() - placed < SETTLE) {
        runAppKitOnce();
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

// What the OS says of the window now, for a failure to say: AppKit's frame against the
// window server's, the room its screen leaves a window, all in points, the content in px,
// and whether the window and the app are the active ones
[[nodiscard]] inline std::string placementOf(const Ui::IWindow & window)
{
    NSWindow * nsWindow = [window.nativeHandle() window];
    return "frame " + textOf(topDownOf([nsWindow frame])) + ", server " + textOf(serverBoundsOf(nsWindow))
         + ", content " + textOf(nativeContentRectOf(window)) + ", visible "
         + textOf(topDownOf([[nsWindow screen] visibleFrame])) + ([nsWindow isKeyWindow] != NO ? ", key" : ", not key")
         + ([NSApp isActive] != NO ? ", active" : ", not active");
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

// Moved and sized by delta's px at once, its notifications sent before this returns. A
// frame is in points up from the bottom-left, which a resize keeps: its top edge moves
inline void moveResizeBy(const Ui::IWindow & window, const Ui::Res::Type::bound_t & delta)
{
    NSWindow *    nsWindow = [window.nativeHandle() window];
    const CGFloat scale    = [nsWindow backingScaleFactor];
    const NSRect  frame    = [nsWindow frame];
    [nsWindow setFrame:NSMakeRect(NSMinX(frame) + (delta.x / scale),
                                  NSMinY(frame) - (delta.y / scale),
                                  NSWidth(frame) + (delta.w / scale),
                                  NSHeight(frame) + (delta.h / scale))
               display:YES];
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

// What the OS says of the window now, for a failure to say
[[nodiscard]] inline std::string placementOf(const Ui::IWindow & window)
{
    return "content " + textOf(nativeContentRectOf(window));
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

// Moved and sized by delta's px, synced, so what the server sends for it waits in the
// queue the next poll reads
inline void moveResizeBy(const Ui::IWindow & window, const Ui::Res::Type::bound_t & delta)
{
    Display *         display = window.nativeDisplay();
    XWindowAttributes attributes {};
    XGetWindowAttributes(display, window.nativeHandle(), &attributes);
    XMoveResizeWindow(display,
                      window.nativeHandle(),
                      attributes.x + Ui::roundToInt(delta.x),
                      attributes.y + Ui::roundToInt(delta.y),
                      static_cast<unsigned int>(attributes.width + Ui::roundToInt(delta.w)),
                      static_cast<unsigned int>(attributes.height + Ui::roundToInt(delta.h)));
    XSync(display, Ui::Window::Platform::X11::False);
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

// The app made inactive, as a click on another app's window makes it, or active again.
// False when the OS kept it as it was
#ifdef _WIN32

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

#elifdef HAVE_X11

// The input focus put on the root window, which no app owns, or back on this one
[[nodiscard]] inline bool setActive(const Ui::IWindow & window, bool isActive)
{
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

// The window put at another DPI, as its OS puts it when the window moves to another monitor
// or the desktop's scale setting changes. False where the OS has no way to
#ifdef _WIN32

// The rect WM_DPICHANGED suggests, through its LPARAM as Windows passes it: a pointer-to-
// integer conversion static_cast cannot express, the sanctioned exception
inline LPARAM asLParam(const RECT * rect)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<LPARAM>(rect);
}

// WM_DPICHANGED sent as Windows sends it: the DPI for both axes, and the frame grown with it
[[nodiscard]] inline bool setDpi(const Ui::IWindow & window, int dpi)
{
    RECT frame {};
    GetWindowRect(window.nativeHandle(), &frame);
    const int  before = window.dpi();
    const RECT suggested { .left   = frame.left,
                           .top    = frame.top,
                           .right  = frame.left + MulDiv(frame.right - frame.left, dpi, before),
                           .bottom = frame.top + MulDiv(frame.bottom - frame.top, dpi, before) };
    SendMessageW(window.nativeHandle(), WM_DPICHANGED, MAKEWPARAM(dpi, dpi), asLParam(&suggested));
    return true;
}

#elifdef __APPLE__

// A backing scale is its screen's alone: only another screen gives a window another
[[nodiscard]] inline bool setDpi(const Ui::IWindow & /*window*/, int /*dpi*/) { return false; }

#elifdef HAVE_X11

// Xft.dpi written into the root window's RESOURCE_MANAGER as xrdb writes it, every other
// resource kept
[[nodiscard]] inline bool setDpi(const Ui::IWindow & window, int dpi)
{
    Display *      display   = window.nativeDisplay();
    const ::Window root      = DefaultRootWindow(display);
    const Atom     resources = XInternAtom(display, "RESOURCE_MANAGER", Ui::Window::Platform::X11::False);
    Atom           type {};
    int            format {};
    Ui::Window::Platform::X11::Cardinal count {};
    Ui::Window::Platform::X11::Cardinal after {};
    unsigned char *                     data = nullptr;
    std::string                         kept;
    if (XGetWindowProperty(display,
                           root,
                           resources,
                           0,
                           65536 / 4,
                           Ui::Window::Platform::X11::False,
                           XA_STRING,
                           &type,
                           &format,
                           &count,
                           &after,
                           &data)
        == 0
        && data != nullptr) {
        // Xlib ends the data with a NUL, so it reads as a C string
        const std::string text = Common::fromCString(data);
        XFree(data);
        std::string_view rest = text;
        while (!rest.empty()) {
            const std::size_t      end  = std::min(rest.find('\n'), rest.size());
            const std::string_view line = rest.substr(0, end);
            if (!line.starts_with("Xft.dpi:")) {
                kept += line;
                kept += '\n';
            }
            rest.remove_prefix(std::min(end + 1, rest.size()));
        }
    }
    kept += "Xft.dpi:\t" + std::to_string(dpi) + "\n";
    XChangeProperty(display,
                    root,
                    resources,
                    XA_STRING,
                    8,
                    PropModeReplace,
                    Common::asBytes(kept.data()),
                    static_cast<int>(kept.size()));
    XSync(display, Ui::Window::Platform::X11::False);
    return true;
}

#endif

} // namespace PureGlUi
