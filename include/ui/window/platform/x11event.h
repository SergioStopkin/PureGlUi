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
#include "ui/interface/ieventos.h"
#include "ui/interface/iwindow.h"
#include "ui/window/clickcounter.h"
#include "ui/window/event.h"
#include "ui/window/platform/x11include.h"

#include <algorithm>
#include <array>
#include <cstring>
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
constexpr int CONFIGURE_NOTIFY  = 22;
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
        auto * display  = window.nativeDisplay();
        m_wmDeleteAtom  = XInternAtom(display, "WM_DELETE_WINDOW", X11::False);
        m_clipboardAtom = XInternAtom(display, "CLIPBOARD", X11::False);
        m_targetsAtom   = XInternAtom(display, "TARGETS", X11::False);
        m_utf8Atom      = XInternAtom(display, "UTF8_STRING", X11::False);
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

private:
    Atom        m_wmDeleteAtom  = 0;
    Atom        m_clipboardAtom = 0;
    Atom        m_targetsAtom   = 0;
    Atom        m_utf8Atom      = 0;
    std::string m_clipboardText;

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
        case X11EventTypes::BUTTON_RELEASE: eventWindow = xev.xbutton.window; break;
        case X11EventTypes::MOTION_NOTIFY: eventWindow = xev.xmotion.window; break;
        case X11EventTypes::LEAVE_NOTIFY: eventWindow = xev.xcrossing.window; break;
        case X11EventTypes::CONFIGURE_NOTIFY: eventWindow = xev.xconfigure.window; break;
        case X11EventTypes::KEY_PRESS:
        case X11EventTypes::KEY_RELEASE: eventWindow = xev.xkey.window; break;
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

        case X11EventTypes::CONFIGURE_NOTIFY:
            if (xev.xconfigure.window == mainWin) {
                event.type          = EventType::Resize;
                event.resize.width  = xev.xconfigure.width;
                event.resize.height = xev.xconfigure.height;
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

    /**
     * @brief Convert X11 modifier state to platform-agnostic modifiers
     */
    static KeyModifier convertX11Modifiers(unsigned int state)
    {
        KeyModifier mods = KeyModifier::None;

        if (Common::Bit::And(state, ShiftMask) != 0U) {
            mods = mods | KeyModifier::Shift;
        }
        if (Common::Bit::And(state, ControlMask) != 0U) {
            mods = mods | KeyModifier::Control;
        }
        if (Common::Bit::And(state, Mod1Mask) != 0U) { // Alt
            mods = mods | KeyModifier::Alt;
        }

        return mods;
    }
};

} // namespace Ui::Window::Platform
