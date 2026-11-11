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

#include "common/bit.h"
#include "common/bytes.h"
#include "common/cfree.h"
#include "ui/interface/ieventos.h"
#include "ui/interface/iwindow.h"
#include "ui/window/clickcounter.h"
#include "ui/window/event.h"
#include "ui/window/eventtype.h"
#include "ui/window/keymodifier.h"
#include "ui/window/mousebutton.h"
#include "ui/window/nativewindowhandle.h"
#include "ui/window/platform/x11include.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

// X11 protocol event types (values from X.h). SCREAMING_CASE constexpr so the
// names don't collide with X.h's ButtonPress/SelectionNotify/... macros - no _
// suffix and no #undef needed, and switch(xev.type) stays cast-free.
namespace Ui::Window::Platform::X11EventTypes {
constexpr int KEY_PRESS         = 2;
constexpr int KEY_RELEASE       = 3;
constexpr int BUTTON_PRESS      = 4;
constexpr int BUTTON_RELEASE    = 5;
constexpr int MOTION_NOTIFY     = 6;
constexpr int LEAVE_NOTIFY      = 8;
constexpr int FOCUS_OUT         = 10;
constexpr int MAP_NOTIFY        = 19;
constexpr int CONFIGURE_NOTIFY  = 22;
constexpr int PROPERTY_NOTIFY   = 28;
constexpr int SELECTION_REQUEST = 30;
constexpr int SELECTION_NOTIFY  = 31;
constexpr int CLIENT_MESSAGE    = 33;
} // namespace Ui::Window::Platform::X11EventTypes

namespace Ui::Window::Platform {

/**
 * @brief X11 event handling implementation
 *
 * Implements Ui::IEventOS for X11/Linux platform.
 * Does NOT own any windows - receives them as parameters.
 * Handles XEvent conversion to platform-agnostic Event struct.
 */
class X11Event final : public Ui::IEventOS {
public:
    X11Event()           = default;
    ~X11Event() override = default;

    /**
     * @brief Initialize with WM_DELETE_WINDOW atom
     * @param window Main window to get display from
     */
    void init(Ui::IWindow & window) override
    {
        auto * display     = window.nativeDisplay();
        m_wmDeleteAtom     = XInternAtom(display, "WM_DELETE_WINDOW", X11::False);
        m_clipboardAtom    = XInternAtom(display, "CLIPBOARD", X11::False);
        m_targetsAtom      = XInternAtom(display, "TARGETS", X11::False);
        m_utf8Atom         = XInternAtom(display, "UTF8_STRING", X11::False);
        m_activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", X11::False);
        m_pidAtom          = XInternAtom(display, "_NET_WM_PID", X11::False);

        // Xft.dpi lives in the root window's RESOURCE_MANAGER, which xrdb and every
        // desktop's scale setting rewrite: each rewrite is a scale the UI may follow
        m_resourceManagerAtom = XInternAtom(display, "RESOURCE_MANAGER", X11::False);
        XSelectInput(display, DefaultRootWindow(display), PropertyChangeMask);
    }

    // WindowManager-provided native-handle -> child id lookup, called in
    // convertX11Event() to stamp event.childWindowId.
    void setChildWindowLookup(Ui::Window::child_id_fn_t lookup) override { m_childWindowLookup = std::move(lookup); }

    /**
     * @brief Register the popup window for event routing
     * @param handle Native window handle (or 0 to clear)
     */
    void addPopupWindow(NativeWindowHandle /*handle*/) override { }
    void removePopupWindow(NativeWindowHandle /*handle*/) override { }

    // -------- Ui::IEventOS implementation --------

    [[nodiscard]] bool hasPendingEvents(Ui::IWindow & window) const override
    {
        auto * display = window.nativeDisplay();
        if (display == nullptr) {
            return false;
        }
        return XPending(display) > 0;
    }

    bool pollEvent(Event & event, Ui::IWindow & window) override
    {
        auto * display = window.nativeDisplay();
        if (display == nullptr || XPending(display) == 0) {
            return false;
        }

        XEvent xev;
        XNextEvent(display, &xev);

        auto mainWin = window.nativeHandle();
        event        = convertX11Event(xev, mainWin);
        return true;
    }

    void flush(Ui::IWindow & window) override
    {
        auto * display = window.nativeDisplay();
        if (display != nullptr) {
            XFlush(display);
        }
    }

    void setDoubleClickConfig(uint32_t intervalMs, int distancePx) override
    {
        m_clickCounter.configure(intervalMs, distancePx);
    }

    bool copyToClipboard(Ui::IWindow & window, const std::string & text) override
    {
        auto * display = window.nativeDisplay();
        auto   win     = window.nativeHandle();
        if (display == nullptr || win == 0) {
            return false;
        }

        m_clipboardText = text;
        XSetSelectionOwner(display, m_clipboardAtom, win, CurrentTime);
        XFlush(display);

        return XGetSelectionOwner(display, m_clipboardAtom) == win;
    }

    // Every top-level window's map is watched while a dialog is awaited, to hand the dialog the
    // focus: the desktop holds it back, the dialog's own last input older than ours that asked
    void setDialogAwaited(Ui::IWindow & window, bool isAwaited) override
    {
        if (isAwaited == m_isDialogAwaited) {
            return;
        }
        m_isDialogAwaited  = isAwaited;
        auto *     display = window.nativeDisplay();
        const auto mask = isAwaited ? Common::Bit::Or(PropertyChangeMask, SubstructureNotifyMask) : PropertyChangeMask;
        XSelectInput(display, DefaultRootWindow(display), mask);
    }

private:
    static constexpr int       LONGS    = 32; // a property's or client message's format: longs
    static constexpr X11::Long ONE_LONG = 1;  // what a property read asks for, in longs
    static constexpr X11::Long FROM_APPLICATION =
    1;                                         // _NET_ACTIVE_WINDOW's source: an application, held to the focus rules
    static constexpr X11::Long FROM_FIRST = 0; // a property read's offset
    static constexpr ::Window  NO_WINDOW {};   // none found

    Atom        m_wmDeleteAtom        = 0;
    Atom        m_clipboardAtom       = 0;
    Atom        m_targetsAtom         = 0;
    Atom        m_utf8Atom            = 0;
    Atom        m_resourceManagerAtom = 0;
    Atom        m_activeWindowAtom    = 0;
    Atom        m_pidAtom             = 0;
    std::string m_clipboardText;
    Time        m_inputTime = CurrentTime; // our latest key or button, which a dialog asked by it is activated with
    bool        m_isDialogAwaited = false;

    // Double-click reconstruction (X11 has no native double-click signal).
    ClickCounter m_clickCounter;

    Ui::Window::child_id_fn_t m_childWindowLookup;

    /**
     * @brief Convert X11 event to platform-agnostic event
     */
    Event convertX11Event(const XEvent & xev, ::Window mainWin)
    {
        Event event;

        // Determine which window the event is for
        ::Window eventWindow = 0;
        switch (xev.type) {
        case X11EventTypes::BUTTON_PRESS:
        case X11EventTypes::BUTTON_RELEASE:
            eventWindow = xev.xbutton.window;
            m_inputTime = xev.xbutton.time;
            break;
        case X11EventTypes::MOTION_NOTIFY: eventWindow = xev.xmotion.window; break;
        case X11EventTypes::LEAVE_NOTIFY: eventWindow = xev.xcrossing.window; break;
        case X11EventTypes::CONFIGURE_NOTIFY: eventWindow = xev.xconfigure.window; break;
        case X11EventTypes::KEY_PRESS:
        case X11EventTypes::KEY_RELEASE:
            eventWindow = xev.xkey.window;
            m_inputTime = xev.xkey.time;
            break;
        default: eventWindow = mainWin; break;
        }

        // Store source window for WindowManager to classify
        event.sourceWindow = eventWindow;

        // Attribute the event to its child (content) surface so the dispatcher can
        // route it; a main-window event resolves to nothing and keeps childWindowId
        // = INVALID_ID. (default-case events already fall back to mainWin above.)
        if (m_childWindowLookup) {
            event.childWindowId = m_childWindowLookup(eventWindow);
        }

        // Convert event type and data
        switch (xev.type) {
        case X11EventTypes::CLIENT_MESSAGE: {
            const XClientMessageEvent & clientMsg = xev.xclient;
            // Direct union member access required by X11 API
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            const Atom messageAtom = static_cast<Atom>(clientMsg.data.l[0]);
            if (messageAtom == m_wmDeleteAtom) {
                event.type = EventType::CloseRequested;
            }
        } break;

        // The main window's own, never its copy the root's watch brings while a dialog is awaited
        case X11EventTypes::CONFIGURE_NOTIFY:
            if (xev.xconfigure.window == mainWin && xev.xconfigure.event == mainWin) {
                event.type          = EventType::Resize;
                event.resize.width  = xev.xconfigure.width;
                event.resize.height = xev.xconfigure.height;
            }
            break;

        // The focus gone from the main window and everything in it: not to a child of its
        // own (Inferior), not a grab's passing hold (Grab/Ungrab), not the pointer's
        // following in PointerRoot mode (Pointer) - another app, or none, has it
        case X11EventTypes::FOCUS_OUT:
            if (xev.xfocus.window == mainWin
                && (xev.xfocus.mode == NotifyNormal || xev.xfocus.mode == NotifyWhileGrabbed)
                && xev.xfocus.detail != NotifyInferior && xev.xfocus.detail != NotifyPointer) {
                event.type = EventType::FocusLost;
            }
            break;

        // A top-level window mapped, seen through the root while a dialog is awaited; never a
        // menu or tooltip, which a window manager does not focus
        case X11EventTypes::MAP_NOTIFY:
            if (m_isDialogAwaited && xev.xmap.event == DefaultRootWindow(xev.xmap.display)
                && xev.xmap.override_redirect == X11::False) {
                activateDialogIn(xev.xmap.display, xev.xmap.window, mainWin);
            }
            break;

        // Stamped as the main window's (the default above): the scale is the whole UI's
        case X11EventTypes::PROPERTY_NOTIFY:
            if (xev.xproperty.atom == m_resourceManagerAtom) {
                event.type = EventType::ScaleChange;
            }
            break;

        case X11EventTypes::BUTTON_PRESS:
            if (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) {
                // X11 scroll: Button4 = up, Button5 = down
                event.type          = EventType::Scroll;
                event.mouse.x       = xev.xbutton.x;
                event.mouse.y       = xev.xbutton.y;
                event.scroll.deltaY = (xev.xbutton.button == Button5) ? 1.0F : -1.0F;
            } else {
                event.type             = EventType::MouseButtonPress;
                event.mouse.x          = xev.xbutton.x;
                event.mouse.y          = xev.xbutton.y;
                event.mouse.button     = toMouseButton(xev.xbutton.button);
                event.mouse.clickCount = m_clickCounter.next(static_cast<uint32_t>(xev.xbutton.time),
                                                             xev.xbutton.x,
                                                             xev.xbutton.y,
                                                             xev.xbutton.button);
                event.mouse.modifiers  = convertX11Modifiers(xev.xbutton.state);
            }
            break;

        case X11EventTypes::BUTTON_RELEASE:
            // X11 sends ButtonRelease for scroll buttons Button4/Button5 - ignore them
            if (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) {
                break;
            }
            event.type         = EventType::MouseButtonRelease;
            event.mouse.x      = xev.xbutton.x;
            event.mouse.y      = xev.xbutton.y;
            event.mouse.button = toMouseButton(xev.xbutton.button);
            break;

        case X11EventTypes::MOTION_NOTIFY:
            event.type    = EventType::MouseMove;
            event.mouse.x = xev.xmotion.x;
            event.mouse.y = xev.xmotion.y;
            break;

        case X11EventTypes::LEAVE_NOTIFY: event.type = EventType::MouseLeave; break;

        case X11EventTypes::KEY_PRESS: {
            event.type       = EventType::KeyPress;
            event.key.keysym = XLookupKeysym(
            const_cast<XKeyEvent *>(&xev.xkey), // NOLINT(cppcoreguidelines-pro-type-const-cast)
            0);
            event.key.modifiers = convertX11Modifiers(xev.xkey.state);

            // Resolve logical key name (e.g. "Return", "F1", "q")
            if (const char * keyname = XKeysymToString(event.key.keysym); keyname != nullptr) {
                std::strncpy(event.key.name.data(), keyname, event.key.name.size() - 1);
            }

            // Get text representation
            KeySym              keysym = 0;
            std::array<char, 8> buffer = {};
            XLookupString(const_cast<XKeyEvent *>(&xev.xkey), // NOLINT(cppcoreguidelines-pro-type-const-cast)
                          buffer.data(),
                          static_cast<int>(buffer.size() - 1),
                          &keysym,
                          nullptr);
            event.key.text = buffer;
        } break;

        case X11EventTypes::KEY_RELEASE: {
            event.type       = EventType::KeyRelease;
            event.key.keysym = XLookupKeysym(
            const_cast<XKeyEvent *>(&xev.xkey), // NOLINT(cppcoreguidelines-pro-type-const-cast)
            0);
            event.key.modifiers = convertX11Modifiers(xev.xkey.state);
        } break;

        case X11EventTypes::SELECTION_REQUEST: {
            const XSelectionRequestEvent & req = xev.xselectionrequest;
            // Build the reply inside the XEvent union so XSendEvent needs no cast.
            XEvent            respEvent {};
            XSelectionEvent & resp = respEvent.xselection;
            resp.type              = X11EventTypes::SELECTION_NOTIFY;
            resp.requestor         = req.requestor;
            resp.selection         = req.selection;
            resp.target            = req.target;
            resp.time              = req.time;
            resp.property          = 0;

            auto * display = req.display;
            if (req.target == m_targetsAtom) {
                // Report supported targets
                std::array<Atom, 3> targets = { m_targetsAtom, m_utf8Atom, XA_STRING };
                XChangeProperty(display,
                                req.requestor,
                                req.property,
                                XA_ATOM,
                                32,
                                PropModeReplace,
                                Common::asBytes(targets.data()),
                                3);
                resp.property = req.property;
            } else if (req.target == m_utf8Atom || req.target == XA_STRING) {
                // Serve clipboard text
                XChangeProperty(display,
                                req.requestor,
                                req.property,
                                req.target,
                                8,
                                PropModeReplace,
                                Common::asBytes(m_clipboardText.data()),
                                static_cast<int>(m_clipboardText.size()));
                resp.property = req.property;
            }

            XSendEvent(display, req.requestor, X11::False, 0, &respEvent);
            XFlush(display);
        } break;

        default: break;
        }

        return event;
    }

    // The dialog another program made transient for the main window, asked to the front with the
    // focus. Its window can be gone by the time it is asked about, an error the default handler
    // ends the app on, so errors are ignored for these few requests
    void activateDialogIn(Display * display, ::Window mapped, ::Window mainWin) const
    {
        XSync(display, X11::False);
        const XErrorHandler previous = XSetErrorHandler(ignoreError);
        const ::Window      dialog   = dialogIn(display, mapped, mainWin);
        XSync(display, X11::False);
        XSetErrorHandler(previous);
        if (dialog != NO_WINDOW) {
            sendActivation(display, dialog, mainWin);
        }
    }

    // The window mapped, or the client a window manager's frame holds, when another program made
    // it transient for the main window
    [[nodiscard]] ::Window dialogIn(Display * display, ::Window mapped, ::Window mainWin) const
    {
        ::Window   root {};
        ::Window   parent {};
        ::Window * children = nullptr;
        unsigned   count {};
        if (XQueryTree(display, mapped, &root, &parent, &children, &count) == X11::False) {
            return NO_WINDOW;
        }
        const Common::c_ptr_t<::Window, XFree> framed(children);
        const std::span<::Window>              framedWindows(children, count);
        std::vector<::Window>                  candidates { mapped };
        candidates.insert(candidates.end(), framedWindows.begin(), framedWindows.end());
        for (const ::Window candidate : candidates) {
            ::Window owner {};
            if (XGetTransientForHint(display, candidate, &owner) != X11::False && owner == mainWin
                && isAnotherProgramsWindow(display, candidate)) {
                return candidate;
            }
        }
        return NO_WINDOW;
    }

    static int ignoreError(Display * /*display*/, XErrorEvent * /*error*/) { return {}; }

    // Another program's window, by the process it names: ours name none, so a popup of ours,
    // transient for the main window too, is never taken for the dialog
    [[nodiscard]] bool isAnotherProgramsWindow(Display * display, ::Window window) const
    {
        Atom            actualType {};
        int             actualFormat {};
        X11::Cardinal   count {};
        X11::Cardinal   bytesAfter {};
        unsigned char * data = nullptr;
        if (XGetWindowProperty(display,
                               window,
                               m_pidAtom,
                               FROM_FIRST,
                               ONE_LONG,
                               X11::False,
                               XA_CARDINAL,
                               &actualType,
                               &actualFormat,
                               &count,
                               &bytesAfter,
                               &data)
            != Success) {
            return false;
        }
        const Common::c_ptr_t<unsigned char, XFree> owned(data);
        if (data == nullptr || actualFormat != LONGS || count == X11::Cardinal {}) {
            return false;
        }
        // Format 32 comes as a C long
        std::array<unsigned char, sizeof(X11::Long)> bytes {};
        std::copy_n(data, bytes.size(), bytes.begin());
        return std::bit_cast<X11::Long>(bytes) != getpid();
    }

    // _NET_ACTIVE_WINDOW as an application asks it, timed by the input that asked for the dialog:
    // the desktop honours it unless the user has gone on to something else since
    void sendActivation(Display * display, ::Window dialog, ::Window mainWin) const
    {
        if (m_inputTime == CurrentTime) {
            return;
        }
        XEvent                message {};
        XClientMessageEvent & request = message.xclient;
        request.type                  = X11EventTypes::CLIENT_MESSAGE;
        request.window                = dialog;
        request.message_type          = m_activeWindowAtom;
        request.format                = LONGS;
        const std::array data { FROM_APPLICATION,
                                static_cast<X11::Long>(m_inputTime),
                                static_cast<X11::Long>(mainWin) };
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        std::ranges::copy(data, std::begin(request.data.l));
        XSendEvent(display,
                   DefaultRootWindow(display),
                   X11::False,
                   Common::Bit::Or(SubstructureRedirectMask, SubstructureNotifyMask),
                   &message);
        XFlush(display);
    }

    /**
     * @brief Convert X11 modifier state to platform-agnostic modifiers
     */
    static key_modifiers_t convertX11Modifiers(unsigned int state)
    {
        key_modifiers_t mods;

        if (Common::Bit::And(state, ShiftMask) != 0U) {
            mods |= KeyModifier::Shift;
        }
        if (Common::Bit::And(state, ControlMask) != 0U) {
            mods |= KeyModifier::Control;
        }
        if (Common::Bit::And(state, Mod1Mask) != 0U) { // Alt
            mods |= KeyModifier::Alt;
        }

        return mods;
    }
};

} // namespace Ui::Window::Platform
