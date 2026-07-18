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

#include "ui/interface/ieventos.h"
#include "ui/interface/iwindow.h"
#include "ui/window/event.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>
#include <vector>

#ifdef __APPLE__
// Workaround for Xcode 16.4 SDK regression: Icons.h in HIServices uses CALLBACK_API
// which is undefined in 64-bit C++ mode, causing "use of undeclared identifier" errors.
// Defining __ICONS__ prevents the broken Icons.h from being processed.
#define __ICONS__
#import <Cocoa/Cocoa.h>
#endif

namespace Ui::Window::Platform {

// Shared close-request flag. Set by NSWindowWillCloseNotification / NSApplicationWillTerminateNotification
// observers registered in MacOsWindow::create(); consumed by MacOsEvent::pollEvent() to emit CloseRequested.
inline std::atomic<bool> g_macCloseRequested { false };

// Shared resize snapshot. Set by NSWindowDidResizeNotification observer in physical pixels;
// consumed by MacOsEvent::pollEvent() as a coalesced Resize event.
inline std::atomic<bool> g_macResizePending { false };
inline std::atomic<int>  g_macResizeWidth { 0 };
inline std::atomic<int>  g_macResizeHeight { 0 };

#ifdef __APPLE__
// Retina multiplier. NSWindow frames and NSEvent locations are in POINTS; the rest of the codebase
// works in physical pixels. Multiply point-values by this to convert to pixels.
inline CGFloat macBackingScaleFactor()
{
    NSScreen * screen = [NSScreen mainScreen];
    return screen ? [screen backingScaleFactor] : 1.0;
}
#endif

/**
 * @brief macOS event handling implementation
 *
 * Implements Ui::IEventOS for macOS platform.
 * Does NOT own any windows - receives them as parameters.
 * Handles NSEvent conversion to platform-agnostic Event struct.
 */
class MacOsEvent final : public Ui::IEventOS {
public:
    MacOsEvent()           = default;
    ~MacOsEvent() override = default;

    /**
     * @brief Initialize (no-op for macOS)
     */
    void init(Ui::IWindow & /*window*/) override { }

    // WindowManager-provided native-handle -> child id lookup, called in
    // convertNSEvent() to stamp event.childWindowId.
    void setChildWindowLookup(Ui::Window::child_id_fn_t lookup) override { m_childWindowLookup = std::move(lookup); }

    void addPopupWindow(NativeWindowHandle /*handle*/) override { }
    void removePopupWindow(NativeWindowHandle /*handle*/) override { }

    // -------- Ui::IEventOS implementation --------

    [[nodiscard]] bool hasPendingEvents(Ui::IWindow & window) const override
    {
        (void)window;
#ifdef __APPLE__
        @autoreleasepool {
            NSEvent * event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:nil
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:NO];
            return event != nil;
        }
#else
        return false;
#endif
    }

    bool pollEvent(Event & event, Ui::IWindow & window) override
    {
        (void)window;
#ifdef __APPLE__
        // Synthesize CloseRequested from the shared flag (set by window/app notification observers).
        if (g_macCloseRequested.exchange(false)) {
            event      = Event {};
            event.type = EventType::CloseRequested;
            return true;
        }

        // Synthesize coalesced Resize from the shared snapshot (set by NSWindowDidResizeNotification observer).
        if (g_macResizePending.exchange(false)) {
            event               = Event {};
            event.type          = EventType::Resize;
            event.resize.width  = static_cast<fpx_t>(g_macResizeWidth.load());
            event.resize.height = static_cast<fpx_t>(g_macResizeHeight.load());
            return true;
        }

        @autoreleasepool {
            NSEvent * nsEvent = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                   untilDate:nil
                                                      inMode:NSDefaultRunLoopMode
                                                     dequeue:YES];
            if (nsEvent == nil) {
                return false;
            }

            event = convertNSEvent(nsEvent);

            // Let NSApp handle the event for proper window management
            [NSApp sendEvent:nsEvent];

            return true;
        }
#else
        (void)event;
        return false;
#endif
    }

    void flush(Ui::IWindow & window) override
    {
        (void)window;
        // macOS doesn't need explicit flush like X11
    }

    bool copyToClipboard(Ui::IWindow & window, const std::string & text) override
    {
        (void)window;
#ifdef __APPLE__
        @autoreleasepool {
            NSPasteboard * pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            NSString * str = [NSString stringWithUTF8String:text.c_str()];
            return [pb setString:str forType:NSPasteboardTypeString];
        }
#else
        (void)text;
        return false;
#endif
    }

private:
#ifdef __APPLE__
    /**
     * @brief Convert NSEvent to platform-agnostic event
     */
    static void setMouseXY(Event & event, NSEvent * nsEvent)
    {
        // locationInWindow: points, bottom-left origin.
        // Target:           pixels, top-left origin (matches X11/Wayland/Win32).
        const NSPoint loc   = [nsEvent locationInWindow];
        const CGFloat bsf   = macBackingScaleFactor();
        const NSSize  winSz = [[[nsEvent window] contentView] bounds].size; // points
        event.mouse.x       = static_cast<int>(loc.x * bsf);
        event.mouse.y       = static_cast<int>((winSz.height - loc.y) * bsf);
    }

    Event convertNSEvent(NSEvent * nsEvent)
    {
        Event event;

        // Stamp the source as the NSView (matches NativeWindowHandle = NSView*).
        // For embedded content surfaces the NSEvent targets the main window; hit-testing
        // would be needed to distinguish - deferred until pointer interaction is wired up.
        NSWindow * srcWin  = [nsEvent window];
        NSView *   srcView = [srcWin contentView];
        event.sourceWindow = srcView;
        if (m_childWindowLookup) {
            event.childWindowId = m_childWindowLookup(srcView);
        }

        switch ([nsEvent type]) {
        case NSEventTypeLeftMouseDown:
            event.type = EventType::MouseButtonPress;
            setMouseXY(event, nsEvent);
            event.mouse.button     = MouseButton::Left;
            event.mouse.clickCount = static_cast<int>([nsEvent clickCount]);
            break;

        case NSEventTypeLeftMouseUp:
            event.type = EventType::MouseButtonRelease;
            setMouseXY(event, nsEvent);
            event.mouse.button = MouseButton::Left;
            break;

        case NSEventTypeRightMouseDown:
            event.type = EventType::MouseButtonPress;
            setMouseXY(event, nsEvent);
            event.mouse.button     = MouseButton::Right;
            event.mouse.clickCount = static_cast<int>([nsEvent clickCount]);
            break;

        case NSEventTypeRightMouseUp:
            event.type = EventType::MouseButtonRelease;
            setMouseXY(event, nsEvent);
            event.mouse.button = MouseButton::Right;
            break;

        case NSEventTypeOtherMouseDown:
            event.type = EventType::MouseButtonPress;
            setMouseXY(event, nsEvent);
            event.mouse.button     = MouseButton::Middle;
            event.mouse.clickCount = static_cast<int>([nsEvent clickCount]);
            break;

        case NSEventTypeOtherMouseUp:
            event.type = EventType::MouseButtonRelease;
            setMouseXY(event, nsEvent);
            event.mouse.button = MouseButton::Middle;
            break;

        case NSEventTypeScrollWheel: {
            event.type = EventType::Scroll;
            setMouseXY(event, nsEvent);
            // Normalise to the per-notch convention used by X11 (deltaY is
            // exactly +-1.0 per ButtonPress 4/5) and Win32 (delta / WHEEL_DELTA
            // is +-1.0 per notch). NSEvent has two scroll modes:
            //   - hasPreciseScrollingDeltas == NO (wheel mouse, Magic Mouse
            //     scroll ball): scrollingDeltaY already approx +-1.0 per
            //     notch (sometimes +-3.0 for rapid multi-notch). Pass through;
            //     the renderer's pow(factor, abs(deltaY)) handles the multi.
            //   - hasPreciseScrollingDeltas == YES (trackpad / Magic Trackpad):
            //     scrollingDeltaY is in *points of finger movement*, typically
            //     10-50 per event during inertia and totalling 100+ across one
            //     swipe. Without normalisation the renderer applies dozens of
            //     zoom steps per event - far too fast. Divide by an empirical
            //     constant so a typical swipe ends up applying a handful of
            //     zoom steps, matching the X11/Win32 feel.
            // Sign flip: NSEvent positive scrollingDeltaY is fingers/wheel up;
            // renderer convention is that negative deltaY means zoom-in.
            constexpr CGFloat PRECISE_TO_NOTCH = 300.0;
            const CGFloat     rawDeltaY        = [nsEvent scrollingDeltaY];
            const CGFloat     deltaY = [nsEvent hasPreciseScrollingDeltas] ? (rawDeltaY / PRECISE_TO_NOTCH) : rawDeltaY;
            event.scroll.deltaY      = static_cast<fpx_t>(-deltaY);
        } break;

        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
            event.type = EventType::MouseMove;
            setMouseXY(event, nsEvent);
            break;

        case NSEventTypeKeyDown:
            event.type          = EventType::KeyPress;
            event.key.keysym    = [nsEvent keyCode];
            event.key.modifiers = convertNSModifiers([nsEvent modifierFlags]);
            fillKeyName(event, nsEvent);
            fillKeyText(event, nsEvent);
            break;

        case NSEventTypeKeyUp:
            event.type          = EventType::KeyRelease;
            event.key.keysym    = [nsEvent keyCode];
            event.key.modifiers = convertNSModifiers([nsEvent modifierFlags]);
            fillKeyName(event, nsEvent);
            break;

        default: break;
        }

        return event;
    }

    /**
     * @brief Convert NSEvent modifier flags to platform-agnostic modifiers.
     *
     * Command maps to Control so shortcuts defined as "Ctrl+X" (shared with X11/Win32) match
     * when a macOS user presses Cmd+X. The physical Control key also maps to Control for
     * external-keyboard users; both cases set the same flag. Meta is left unused on macOS.
     */
    static KeyModifier convertNSModifiers(NSEventModifierFlags flags)
    {
        KeyModifier mods = KeyModifier::None;

        if (flags & NSEventModifierFlagShift) {
            mods = mods | KeyModifier::Shift;
        }
        if (flags & (NSEventModifierFlagControl | NSEventModifierFlagCommand)) {
            mods = mods | KeyModifier::Control;
        }
        if (flags & NSEventModifierFlagOption) { // Alt/Option
            mods = mods | KeyModifier::Alt;
        }

        return mods;
    }

    /**
     * @brief Map macOS virtual keycodes to the logical key names used by shortcuts.
     * Returns nullptr for keys that should fall back to charactersIgnoringModifiers.
     */
    static const char * specialKeyName(unsigned short keyCode)
    {
        switch (keyCode) {
        case 0x24: return "Return";    // kVK_Return
        case 0x4C: return "Return";    // kVK_ANSI_KeypadEnter
        case 0x30: return "Tab";       // kVK_Tab
        case 0x31: return "space";     // kVK_Space
        case 0x33: return "BackSpace"; // kVK_Delete (Backspace key)
        case 0x35: return "Escape";    // kVK_Escape
        case 0x75: return "Delete";    // kVK_ForwardDelete
        case 0x73: return "Home";      // kVK_Home
        case 0x77: return "End";       // kVK_End
        case 0x74: return "Page_Up";   // kVK_PageUp
        case 0x79: return "Page_Down"; // kVK_PageDown
        case 0x7B: return "Left";      // kVK_LeftArrow
        case 0x7C: return "Right";     // kVK_RightArrow
        case 0x7D: return "Down";      // kVK_DownArrow
        case 0x7E: return "Up";        // kVK_UpArrow
        case 0x7A: return "F1";
        case 0x78: return "F2";
        case 0x63: return "F3";
        case 0x76: return "F4";
        case 0x60: return "F5";
        case 0x61: return "F6";
        case 0x62: return "F7";
        case 0x64: return "F8";
        case 0x65: return "F9";
        case 0x6D: return "F10";
        case 0x67: return "F11";
        case 0x6F: return "F12";
        default: return nullptr;
        }
    }

    /**
     * @brief Populate event.key.name - required for toShortcutString() to resolve shortcuts.
     * Special keys use a fixed mapping; for regular keys use charactersIgnoringModifiers so
     * Shift+3 reports "3", not "#" (matches X11 XLookupKeysym behavior).
     */
    static void fillKeyName(Event & event, NSEvent * nsEvent)
    {
        if (const char * special = specialKeyName([nsEvent keyCode])) {
            strncpy(event.key.name.data(), special, event.key.name.size() - 1);
            event.key.name[event.key.name.size() - 1] = '\0';
            return;
        }
        NSString * chars = [nsEvent charactersIgnoringModifiers];
        if (chars && [chars length] > 0) {
            const char * utf8 = [chars UTF8String];
            if (utf8) {
                strncpy(event.key.name.data(), utf8, event.key.name.size() - 1);
                event.key.name[event.key.name.size() - 1] = '\0';
            }
        }
    }

    static void fillKeyText(Event & event, NSEvent * nsEvent)
    {
        NSString * chars = [nsEvent characters];
        if (!chars || [chars length] == 0) {
            return;
        }
        const char * utf8 = [chars UTF8String];
        if (utf8) {
            strncpy(event.key.text.data(), utf8, event.key.text.size() - 1);
            event.key.text[event.key.text.size() - 1] = '\0';
        }
    }
#endif

    // WindowManager-provided native-handle -> child id lookup, called in
    // convertNSEvent() to stamp event.childWindowId so the main dispatcher can
    // route content-surface events correctly.
    Ui::Window::child_id_fn_t m_childWindowLookup;
};

} // namespace Ui::Window::Platform
