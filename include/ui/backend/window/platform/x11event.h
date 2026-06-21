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
#include "ui/backend/window/clickcounter.h"
#include "ui/backend/window/event.h"
#include "ui/backend/window/ieventos.h"
#include "ui/backend/window/iwindow.h"
#include "ui/backend/window/platform/x11include.h"

#include <array>
#include <cstring>
#include <vector>

// X11 event type constants (numeric values to avoid macro conflicts with event.h)
// These match the X11 protocol event types from X.h
namespace Ui::Backend::Window::Platform::X11EventTypes {
constexpr int KeyPress_        = 2;
constexpr int KeyRelease_      = 3;
constexpr int ButtonPress_     = 4;
constexpr int ButtonRelease_   = 5;
constexpr int MotionNotify_    = 6;
constexpr int LeaveNotify_     = 8;
constexpr int ConfigureNotify_ = 22;
constexpr int ClientMessage_   = 33;
} // namespace Ui::Backend::Window::Platform::X11EventTypes

namespace Ui::Backend::Window::Platform {

/**
 * @brief X11 event handling implementation
 *
 * Implements Ui::Backend::Window::IEventOS for X11/Linux platform.
 * Does NOT own any windows - receives them as parameters.
 * Handles XEvent conversion to platform-agnostic Event struct.
 */
class X11Event final : public Ui::Backend::Window::IEventOS {
public:
    X11Event()           = default;
    ~X11Event() override = default;

    /**
     * @brief Initialize with WM_DELETE_WINDOW atom
     * @param window Main window to get display from
     */
    void init(Ui::Backend::Window::IWindow & window) override
    {
        auto * display  = window.nativeDisplay();
        m_wmDeleteAtom  = XInternAtom(display, "WM_DELETE_WINDOW", X11::False);
        m_clipboardAtom = XInternAtom(display, "CLIPBOARD", X11::False);
        m_targetsAtom   = XInternAtom(display, "TARGETS", X11::False);
        m_utf8Atom      = XInternAtom(display, "UTF8_STRING", X11::False);
    }

    /**
     * @brief Register a child window for event routing
     * @param handle Native window handle
     * @param id Child window ID
     */
    void registerChildWindow(id_t id, NativeWindowHandle handle) override { m_childWindows.push_back({ id, handle }); }

    /**
     * @brief Unregister a child window
     * @param handle Native window handle
     */
    void unregisterChildWindow(NativeWindowHandle handle) override
    {
        m_childWindows.erase(std::remove_if(m_childWindows.begin(),
                                            m_childWindows.end(),
                                            [handle](const ChildWindowEntry & e) { return e.window == handle; }),
                             m_childWindows.end());
    }

    /**
     * @brief Register the popup window for event routing
     * @param handle Native window handle (or 0 to clear)
     */
    void addPopupWindow(NativeWindowHandle) override { }
    void removePopupWindow(NativeWindowHandle) override { }

    // -------- Ui::Backend::Window::IEventOS implementation --------

    [[nodiscard]] bool hasPendingEvents(Ui::Backend::Window::IWindow & window) const override
    {
        auto * display = window.nativeDisplay();
        if (display == nullptr) {
            return false;
        }
        return XPending(display) > 0;
    }

    bool pollEvent(Event & event, Ui::Backend::Window::IWindow & window) override
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

    void flush(Ui::Backend::Window::IWindow & window) override
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

    bool copyToClipboard(Ui::Backend::Window::IWindow & window, const std::string & text) override
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

    struct alignas(16) ChildWindowEntry final {
        id_t     id     = Ui::INVALID_ID;
        ::Window window = 0;
    };
    std::vector<ChildWindowEntry> m_childWindows;

    /**
     * @brief Convert X11 event to platform-agnostic event
     */
    Event convertX11Event(const XEvent & xev, ::Window mainWin)
    {
        Event event;

        // Determine which window the event is for
        ::Window eventWindow = 0;
        switch (xev.type) {
        case X11EventTypes::ButtonPress_:
        case X11EventTypes::ButtonRelease_: eventWindow = xev.xbutton.window; break;
        case X11EventTypes::MotionNotify_: eventWindow = xev.xmotion.window; break;
        case X11EventTypes::LeaveNotify_: eventWindow = xev.xcrossing.window; break;
        case X11EventTypes::ConfigureNotify_: eventWindow = xev.xconfigure.window; break;
        case X11EventTypes::KeyPress_:
        case X11EventTypes::KeyRelease_: eventWindow = xev.xkey.window; break;
        default: eventWindow = mainWin; break;
        }

        // Store source window for WindowManager to classify
        event.sourceWindow = eventWindow;

        // Convert event type and data
        switch (xev.type) {
        case X11EventTypes::ClientMessage_: {
            const XClientMessageEvent & clientMsg = xev.xclient;
            // Direct union member access required by X11 API
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            const Atom messageAtom = static_cast<Atom>(clientMsg.data.l[0]);
            if (messageAtom == m_wmDeleteAtom) {
                event.type = EventType::CloseRequested;
            }
        } break;

        case X11EventTypes::ConfigureNotify_:
            if (xev.xconfigure.window == mainWin) {
                event.type          = EventType::Resize;
                event.resize.width  = xev.xconfigure.width;
                event.resize.height = xev.xconfigure.height;
            }
            break;

        case X11EventTypes::ButtonPress_:
            if (xev.xbutton.button == 4 || xev.xbutton.button == 5) {
                // X11 scroll: button 4 = up, button 5 = down
                event.type          = EventType::Scroll;
                event.mouse.x       = xev.xbutton.x;
                event.mouse.y       = xev.xbutton.y;
                event.scroll.deltaY = (xev.xbutton.button == 5) ? 1.0F : -1.0F;
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

        case X11EventTypes::ButtonRelease_:
            // X11 sends ButtonRelease for scroll buttons 4/5 - ignore them
            if (xev.xbutton.button == 4 || xev.xbutton.button == 5) {
                break;
            }
            event.type         = EventType::MouseButtonRelease;
            event.mouse.x      = xev.xbutton.x;
            event.mouse.y      = xev.xbutton.y;
            event.mouse.button = toMouseButton(xev.xbutton.button);
            break;

        case X11EventTypes::MotionNotify_:
            event.type    = EventType::MouseMove;
            event.mouse.x = xev.xmotion.x;
            event.mouse.y = xev.xmotion.y;
            break;

        case X11EventTypes::LeaveNotify_: event.type = EventType::MouseLeave; break;

        case X11EventTypes::KeyPress_: {
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

        case X11EventTypes::KeyRelease_: {
            event.type       = EventType::KeyRelease;
            event.key.keysym = XLookupKeysym(
            const_cast<XKeyEvent *>(&xev.xkey), // NOLINT(cppcoreguidelines-pro-type-const-cast)
            0);
            event.key.modifiers = convertX11Modifiers(xev.xkey.state);
        } break;

        case 30 /* SelectionRequest */: {
            const XSelectionRequestEvent & req = xev.xselectionrequest;
            XSelectionEvent                resp;
            resp.type      = 31 /* SelectionNotify */;
            resp.requestor = req.requestor;
            resp.selection = req.selection;
            resp.target    = req.target;
            resp.time      = req.time;
            resp.property  = 0;

            auto * display = req.display;
            if (req.target == m_targetsAtom) {
                // Report supported targets
                std::array<Atom, 3> targets = { m_targetsAtom, m_utf8Atom, XA_STRING };
                XChangeProperty(
                display,
                req.requestor,
                req.property,
                XA_ATOM,
                32,
                PropModeReplace,
                reinterpret_cast<unsigned char *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                targets.data()),
                3);
                resp.property = req.property;
            } else if (req.target == m_utf8Atom || req.target == XA_STRING) {
                // Serve clipboard text
                XChangeProperty(
                display,
                req.requestor,
                req.property,
                req.target,
                8,
                PropModeReplace,
                reinterpret_cast<const unsigned char *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                m_clipboardText.data()),
                static_cast<int>(m_clipboardText.size()));
                resp.property = req.property;
            }

            XSendEvent(display,
                       req.requestor,
                       X11::False,
                       0,
                       reinterpret_cast<XEvent *>(&resp)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
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

} // namespace Ui::Backend::Window::Platform
