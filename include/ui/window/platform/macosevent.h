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

#include "ui/const.h"
#include "ui/interface/ieventos.h"
#include "ui/interface/iwindow.h"
#include "ui/window/event.h"
#include "ui/window/eventtype.h"

#include <algorithm>
#include <cstring>
#include <iostream>
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

// Flip to true to trace every scroll event: what the device sent, the phase of the
// gesture it belongs to (fingers on or the momentum after them) and the notches it became
constexpr bool SCROLL_DEBUG = false;

/**
 * @brief macOS event handling implementation
 *
 * Implements Ui::IEventOS for macOS platform.
 * Does NOT own any windows - receives them as parameters.
 * Handles NSEvent conversion to platform-agnostic Event struct.
 */
class MacOsEvent final : public Ui::IEventOS {
public:
    MacOsEvent() = default;

    // What it observed ends with it, so a shell after this one hears nothing of its window
    ~MacOsEvent() override
    {
#ifdef __APPLE__
        for (id<NSObject> observer : m_observers) {
            [[NSNotificationCenter defaultCenter] removeObserver:observer];
        }
#endif
    }

    // The window's close, resize, move and scale, and the app's end and its loss of the focus,
    // kept here for the loop to read - per shell, so none reaches the next. A resize is due
    // at once: AppKit fits a window to its screen as it shows it, before this observes anything
    void init(Ui::IWindow & window) override
    {
#ifdef __APPLE__
        m_mainView          = window.nativeHandle();
        NSWindow * nsWindow = [m_mainView window];
        observe(NSWindowWillCloseNotification, nsWindow, ^(NSNotification *) { m_isCloseRequested = true; });
        observe(NSApplicationWillTerminateNotification, NSApp, ^(NSNotification *) { m_isCloseRequested = true; });
        observe(NSApplicationDidResignActiveNotification, NSApp, ^(NSNotification *) { m_isFocusLost = true; });
        // A move too, at the same size: the shell keeps where the window stands from it
        observe(NSWindowDidResizeNotification, nsWindow, ^(NSNotification *) { m_isResizeDue = true; });
        observe(NSWindowDidMoveNotification, nsWindow, ^(NSNotification *) { m_isResizeDue = true; });
        // And fullscreen in or out, whatever the size: the shell keeps the state from a resize
        observe(NSWindowDidEnterFullScreenNotification, nsWindow, ^(NSNotification *) { m_isResizeDue = true; });
        observe(NSWindowDidExitFullScreenNotification, nsWindow, ^(NSNotification *) { m_isResizeDue = true; });
        observe(NSWindowDidChangeBackingPropertiesNotification, nsWindow, ^(NSNotification *) {
            m_isScaleChanged = true;
        });
        m_isResizeDue = true;
#else
        (void)window;
#endif
    }

    // WindowManager-provided native-handle -> child id lookup, called in
    // convertNSEvent() to stamp event.childWindowId.
    void setChildWindowLookup(Ui::Window::child_id_fn_t lookup) override { m_childWindowLookup = std::move(lookup); }

    // Our popups, dialogs and tooltip, so their events are told from AppKit's own windows'
    void addPopupWindow(NativeWindowHandle handle) override { m_popupViews.emplace_back(handle); }
    void removePopupWindow(NativeWindowHandle handle) override { std::erase(m_popupViews, handle); }

    // -------- Ui::IEventOS implementation --------

    [[nodiscard]] bool hasPendingEvents(Ui::IWindow & window) const override
    {
        (void)window;
#ifdef __APPLE__
        if (m_isCloseRequested || m_isFocusLost || m_isScaleChanged || m_isResizeDue) {
            return true;
        }
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
#ifdef __APPLE__
        if (m_isCloseRequested) {
            m_isCloseRequested = false;
            event              = Event {};
            event.type         = EventType::CloseRequested;
            return true;
        }

        if (m_isFocusLost) {
            m_isFocusLost = false;
            event         = Event {};
            event.type    = EventType::FocusLost;
            return true;
        }

        // Before the resize a new backing brings, whose px are read at the new scale
        if (m_isScaleChanged) {
            m_isScaleChanged = false;
            m_isResizeDue    = true;
            event            = Event {};
            event.type       = EventType::ScaleChange;
            return true;
        }

        // The size the window has now, in px: a burst of notifications is one event
        if (m_isResizeDue) {
            m_isResizeDue        = false;
            NSView *      view   = window.nativeHandle();
            const NSSize  points = [view bounds].size;
            const CGFloat scale  = [[view window] backingScaleFactor];
            event                = Event {};
            event.type           = EventType::Resize;
            event.resize.width   = static_cast<fpx_t>(points.width * scale);
            event.resize.height  = static_cast<fpx_t>(points.height * scale);
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
        (void)window;
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
        // A move off every window of ours comes with none: it stands nowhere in them, and
        // read as a position it would be their corner, where the first title stands
        NSWindow * nsWindow = [nsEvent window];
        if (nsWindow == nil) {
            event.mouse.x = Ui::NO_POINTER;
            event.mouse.y = Ui::NO_POINTER;
            return;
        }

        // locationInWindow: points, bottom-left origin.
        // Target:           pixels, top-left origin (matches X11/Wayland/Win32).
        const NSPoint loc   = [nsEvent locationInWindow];
        const CGFloat bsf   = [nsWindow backingScaleFactor];
        const NSSize  winSz = [[nsWindow contentView] bounds].size; // points
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

        // A window not ours is AppKit's to handle, sendEvent still handing it there: the
        // title bar fullscreen reveals over our menu bar would press its first title too
        if (srcWin != nil && srcView != m_mainView && std::ranges::find(m_popupViews, srcView) == m_popupViews.end()) {
            return event;
        }
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
            event.mouse.modifiers  = convertNSModifiers([nsEvent modifierFlags]);
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
            event.mouse.modifiers  = convertNSModifiers([nsEvent modifierFlags]);
            break;

        case NSEventTypeRightMouseUp:
            event.type = EventType::MouseButtonRelease;
            setMouseXY(event, nsEvent);
            event.mouse.button = MouseButton::Right;
            break;

        // "Other" is every button past right; only button 2 is middle, and the rest
        // (back/forward) have no MouseButton, so they stay EventType::None
        case NSEventTypeOtherMouseDown:
            if ([nsEvent buttonNumber] != 2) {
                break;
            }
            event.type = EventType::MouseButtonPress;
            setMouseXY(event, nsEvent);
            event.mouse.button     = MouseButton::Middle;
            event.mouse.clickCount = static_cast<int>([nsEvent clickCount]);
            event.mouse.modifiers  = convertNSModifiers([nsEvent modifierFlags]);
            break;

        case NSEventTypeOtherMouseUp:
            if ([nsEvent buttonNumber] != 2) {
                break;
            }
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
            if constexpr (SCROLL_DEBUG) {
                // phase and momentum are NSEventPhase bits: 1 began, 2 stationary, 4 changed,
                // 8 ended, 16 cancelled, 32 may begin
                std::cout << "[Scroll] t=" << static_cast<long long>([nsEvent timestamp] * 1000.0)
                          << "ms raw=" << rawDeltaY << " line=" << [nsEvent deltaY]
                          << " precise=" << ([nsEvent hasPreciseScrollingDeltas] ? 1 : 0)
                          << " phase=" << [nsEvent phase] << " momentum=" << [nsEvent momentumPhase]
                          << " notches=" << event.scroll.deltaY << std::endl;
            }
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
    static key_modifiers_t convertNSModifiers(NSEventModifierFlags flags)
    {
        key_modifiers_t mods;

        if (flags & NSEventModifierFlagShift) {
            mods |= KeyModifier::Shift;
        }
        if (flags & (NSEventModifierFlagControl | NSEventModifierFlagCommand)) {
            mods |= KeyModifier::Control;
        }
        if (flags & NSEventModifierFlagOption) { // Alt/Option
            mods |= KeyModifier::Alt;
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

    // The windows whose events are ours: the main one, where content surfaces stand too,
    // and each popup, dialog and tooltip while it is open - a handful
    NativeWindowHandle              m_mainView {};
    std::vector<NativeWindowHandle> m_popupViews;

    // What the window's notifications left for the loop; AppKit posts them on this thread
    bool m_isCloseRequested = false;
    bool m_isFocusLost      = false;
    bool m_isScaleChanged   = false;
    bool m_isResizeDue      = false;

#ifdef __APPLE__
    std::vector<id<NSObject>> m_observers; // removed with this handler

    // onPosted run on each name's notification from object, until this handler ends
    void observe(NSNotificationName name, id object, void (^onPosted)(NSNotification *))
    {
        m_observers.emplace_back(
        [[NSNotificationCenter defaultCenter] addObserverForName:name object:object queue:nil usingBlock:onPosted]);
    }
#endif
};

} // namespace Ui::Window::Platform
