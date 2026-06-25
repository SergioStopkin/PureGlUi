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

#ifdef HAVE_WAYLAND

#include "ui/config.h"
#include "ui/interface/ieventos.h"
#include "ui/interface/iwindow.h"
#include "ui/window/clickcounter.h"
#include "ui/window/event.h"
#include "ui/window/platform/waylandwindow.h"

#include <iostream>
#include <poll.h>
#include <queue>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

namespace Ui::Window::Platform {

/**
 * @brief Wayland event handling implementation
 *
 * Implements Ui::IEventOS for Wayland/Linux platform.
 * Uses wl_pointer and wl_keyboard listeners to receive events.
 * Events are queued internally and returned via pollEvent().
 */
class WaylandEvent final : public Ui::IEventOS {
public:
    WaylandEvent() = default;

    ~WaylandEvent() override { cleanup(); }

    /**
     * @brief Initialize with main window to get Wayland seat
     * @param window Main window (must be WaylandWindow)
     */
    void init(Ui::IWindow & window) override
    {
        // Get seat from WaylandWindow
        auto * wlWin = dynamic_cast<WaylandWindow *>(&window);
        if (!wlWin) {
            std::cerr << "[WaylandEvent] Window is not a WaylandWindow" << std::endl;
            return;
        }

        wl_seat * seat = wlWin->wlSeat();
        if (!seat) {
            std::cerr << "[WaylandEvent] No seat available" << std::endl;
            return;
        }

        m_seat = seat;

        // Initialize XKB context BEFORE keyboard setup - the keymap callback
        // (triggered by roundtrip) needs it to parse the compositor's keymap.
        m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (!m_xkbContext) {
            std::cerr << "[WaylandEvent] Failed to create XKB context" << std::endl;
        }

        // The seat capabilities event was already dispatched during the registry
        // roundtrip in WaylandWindow::initRegistry(), before this listener existed.
        // Directly create pointer/keyboard from the seat.
        m_pointer = wl_seat_get_pointer(m_seat);
        if (m_pointer) {
            wl_pointer_add_listener(m_pointer, &s_pointerListener, this);
        }

        m_keyboard = wl_seat_get_keyboard(m_seat);
        if (m_keyboard) {
            wl_keyboard_add_listener(m_keyboard, &s_keyboardListener, this);
        }

        // Roundtrip to receive keymap from compositor (needed for xkb key translation)
        wl_display_roundtrip(wlWin->wlDisplay());

        // Load cursor theme for pointer display
        const int cursorSize = 24 * static_cast<int>(g_config.scale);
        m_cursorTheme        = wl_cursor_theme_load(nullptr, cursorSize, wlWin->wlShm());
        if (m_cursorTheme) {
            m_defaultCursor = wl_cursor_theme_get_cursor(m_cursorTheme, "left_ptr");
            m_cursorSurface = wl_compositor_create_surface(wlWin->wlCompositor());
        }

        std::cout << "[WaylandEvent] Initialized: pointer=" << (m_pointer != nullptr)
                  << " keyboard=" << (m_keyboard != nullptr) << " cursor=" << (m_defaultCursor != nullptr) << std::endl;
    }

    /**
     * @brief Register a child window (subsurface) for event routing
     */
    void registerChildWindow(id_t id, NativeWindowHandle handle) override { m_childWindows.push_back({ id, handle }); }

    /**
     * @brief Unregister a child window
     */
    void unregisterChildWindow(NativeWindowHandle handle) override
    {
        m_childWindows.erase(std::remove_if(m_childWindows.begin(),
                                            m_childWindows.end(),
                                            [handle](const ChildWindowEntry & e) { return e.surface == handle; }),
                             m_childWindows.end());
    }

    /**
     * @brief Register the popup window for event routing
     */
    void addPopupWindow(NativeWindowHandle) override { }

    void removePopupWindow(NativeWindowHandle handle) override
    {
        // Clear focus if it pointed to the removed popup (prevents use-after-free
        // when the popup surface is destroyed before the leave event arrives)
        if (m_focusSurface == handle) {
            m_focusSurface = nullptr;
        }
    }

    // -------- Ui::IEventOS implementation --------

    bool hasPendingEvents(Ui::IWindow & window) const override
    {
        // Check queued events first
        if (!m_eventQueue.empty()) {
            return true;
        }

        // Check Wayland for pending events
        auto * wlWin = dynamic_cast<WaylandWindow *>(&window);
        if (!wlWin) {
            return false;
        }

        wl_display * display = wlWin->wlDisplay();
        if (!display) {
            return false;
        }

        // Check if there's data to read on the socket
        struct pollfd pfd = {};
        pfd.fd            = wl_display_get_fd(display);
        pfd.events        = POLLIN;

        return poll(&pfd, 1, 0) > 0;
    }

    bool pollEvent(Event & event, Ui::IWindow & window) override
    {
        auto * wlWin = dynamic_cast<WaylandWindow *>(&window);
        if (!wlWin) {
            return false;
        }

        wl_display * display = wlWin->wlDisplay();
        if (!display) {
            return false;
        }

        // Flush outgoing requests first
        if (wl_display_flush(display) == -1) {
            // Error flushing - check for protocol error
            if (wl_display_get_error(display) != 0) {
                std::cerr << "[WaylandEvent] Display error: " << wl_display_get_error(display) << std::endl;
                return false;
            }
        }

        // Dispatch any pending events already in the queue
        wl_display_dispatch_pending(display);

        // Check for window resize (compare with cached size)
        int currentWidth  = static_cast<int>(wlWin->bound().w);
        int currentHeight = static_cast<int>(wlWin->bound().h);
        if (m_lastWidth != currentWidth || m_lastHeight != currentHeight) {
            m_lastWidth  = currentWidth;
            m_lastHeight = currentHeight;

            // Only generate resize event if size is valid (not initial 0x0)
            if (currentWidth > 0 && currentHeight > 0) {
                event.type          = EventType::Resize;
                event.resize.width  = currentWidth;
                event.resize.height = currentHeight;
                return true;
            }
        }

        // Return queued event if available
        if (!m_eventQueue.empty()) {
            event = m_eventQueue.front();
            m_eventQueue.pop();
            return true;
        }

        // Check for window close request (after draining the queue so pending
        // events are not lost, and only emit once)
        if (wlWin->shouldClose() && !m_closeEmitted) {
            m_closeEmitted = true;
            event.type     = EventType::CloseRequested;
            return true;
        }

        // If no events queued, try to read from socket (non-blocking)
        // prepare_read returns 0 if we can read, -1 if there are pending events
        while (wl_display_prepare_read(display) != 0) {
            // There are pending events, dispatch them
            wl_display_dispatch_pending(display);
            if (!m_eventQueue.empty()) {
                event = m_eventQueue.front();
                m_eventQueue.pop();
                return true;
            }
        }

        // Now we can read from the socket
        // Use poll to check if there's data to read without blocking
        struct pollfd pfd = {};
        pfd.fd            = wl_display_get_fd(display);
        pfd.events        = POLLIN;

        if (poll(&pfd, 1, 0) > 0) {
            // Data available, read it
            wl_display_read_events(display);
            wl_display_dispatch_pending(display);
        } else {
            // No data, cancel the read
            wl_display_cancel_read(display);
        }

        // Return any new events
        if (!m_eventQueue.empty()) {
            event = m_eventQueue.front();
            m_eventQueue.pop();
            return true;
        }

        return false;
    }

    void flush(Ui::IWindow & window) override
    {
        auto * wlWin = dynamic_cast<WaylandWindow *>(&window);
        if (!wlWin) {
            return;
        }

        wl_display * display = wlWin->wlDisplay();
        if (display) {
            wl_display_flush(display);
        }
    }

    void setDoubleClickConfig(uint32_t intervalMs, int distancePx) override
    {
        m_clickCounter.configure(intervalMs, distancePx);
    }

    bool copyToClipboard(Ui::IWindow & window, const std::string & text) override
    {
        (void)window;
        (void)text;
        // TODO(sergio): wl_data_source implementation
        return false;
    }

private:
    wl_seat *         m_seat          = nullptr;
    wl_pointer *      m_pointer       = nullptr;
    wl_keyboard *     m_keyboard      = nullptr;
    wl_cursor_theme * m_cursorTheme   = nullptr;
    wl_surface *      m_cursorSurface = nullptr;
    wl_cursor *       m_defaultCursor = nullptr;
    uint32_t          m_pointerSerial = 0;
    wl_surface *      m_focusSurface  = nullptr;
    wl_surface *      m_mainSurface   = nullptr;

    // XKB keyboard state
    xkb_context * m_xkbContext = nullptr;
    xkb_keymap *  m_xkbKeymap  = nullptr;
    xkb_state *   m_xkbState   = nullptr;

    // Event queue
    mutable std::queue<Event> m_eventQueue;

    // Current mouse position
    int m_mouseX = 0;
    int m_mouseY = 0;

    // Double-click reconstruction (Wayland has no native signal).
    ClickCounter m_clickCounter;

    // Track window size for generating resize events
    int  m_lastWidth    = 0;
    int  m_lastHeight   = 0;
    bool m_closeEmitted = false;

    struct alignas(16) ChildWindowEntry final {
        id_t         id      = Ui::INVALID_ID;
        wl_surface * surface = nullptr;
    };
    std::vector<ChildWindowEntry> m_childWindows;

    void cleanup()
    {
        if (m_cursorSurface) {
            wl_surface_destroy(m_cursorSurface);
            m_cursorSurface = nullptr;
        }
        if (m_cursorTheme) {
            wl_cursor_theme_destroy(m_cursorTheme);
            m_cursorTheme   = nullptr;
            m_defaultCursor = nullptr;
        }
        if (m_pointer) {
            wl_pointer_destroy(m_pointer);
            m_pointer = nullptr;
        }
        if (m_keyboard) {
            wl_keyboard_destroy(m_keyboard);
            m_keyboard = nullptr;
        }
        if (m_xkbState) {
            xkb_state_unref(m_xkbState);
            m_xkbState = nullptr;
        }
        if (m_xkbKeymap) {
            xkb_keymap_unref(m_xkbKeymap);
            m_xkbKeymap = nullptr;
        }
        if (m_xkbContext) {
            xkb_context_unref(m_xkbContext);
            m_xkbContext = nullptr;
        }
    }

    void queueEvent(const Event & event) { m_eventQueue.push(event); }

    [[nodiscard]] id_t findChildWindowId(wl_surface * surface) const
    {
        if (!surface) {
            return Ui::INVALID_ID;
        }
        for (const auto & child : m_childWindows) {
            if (child.surface == surface) {
                return child.id;
            }
        }
        return Ui::INVALID_ID;
    }

    // -------- Seat listeners --------

    static void seatCapabilities(void * data, wl_seat * seat, uint32_t capabilities)
    {
        auto * self = static_cast<WaylandEvent *>(data);

        bool hasPointer  = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;
        bool hasKeyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;

        if (hasPointer && !self->m_pointer) {
            self->m_pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(self->m_pointer, &s_pointerListener, self);
        } else if (!hasPointer && self->m_pointer) {
            wl_pointer_destroy(self->m_pointer);
            self->m_pointer = nullptr;
        }

        if (hasKeyboard && !self->m_keyboard) {
            self->m_keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(self->m_keyboard, &s_keyboardListener, self);
        } else if (!hasKeyboard && self->m_keyboard) {
            wl_keyboard_destroy(self->m_keyboard);
            self->m_keyboard = nullptr;
        }
    }

    static void seatName(void * /*data*/, wl_seat * /*seat*/, const char * /*name*/) { }

    static constexpr wl_seat_listener s_seatListener = { seatCapabilities, seatName };

    // -------- Pointer listeners --------

    static void
    pointerEnter(void * data, wl_pointer * pointer, uint32_t serial, wl_surface * surface, wl_fixed_t sx, wl_fixed_t sy)
    {
        auto * self           = static_cast<WaylandEvent *>(data);
        self->m_focusSurface  = surface;
        self->m_pointerSerial = serial;
        // Wayland gives surface-local (logical) coords; scale to physical pixels
        self->m_mouseX = toPhysFloor(wl_fixed_to_int(sx));
        self->m_mouseY = toPhysFloor(wl_fixed_to_int(sy));

        // Set cursor - Wayland requires the client to set it on every enter
        if (self->m_defaultCursor && self->m_cursorSurface) {
            wl_cursor_image * image = self->m_defaultCursor->images[0];
            wl_buffer *       buf   = wl_cursor_image_get_buffer(image);
            wl_surface_attach(self->m_cursorSurface, buf, 0, 0);
            wl_surface_damage(self->m_cursorSurface, 0, 0, image->width, image->height);
            wl_surface_commit(self->m_cursorSurface);
            wl_pointer_set_cursor(pointer, serial, self->m_cursorSurface, image->hotspot_x, image->hotspot_y);
        }
    }

    static void pointerLeave(void * data, wl_pointer * /*pointer*/, uint32_t /*serial*/, wl_surface * surface)
    {
        auto * self          = static_cast<WaylandEvent *>(data);
        self->m_focusSurface = nullptr;

        Event event;
        event.type         = EventType::MouseLeave;
        event.sourceWindow = surface;
        self->queueEvent(event);
    }

    static void pointerMotion(void * data, wl_pointer * /*pointer*/, uint32_t /*time*/, wl_fixed_t sx, wl_fixed_t sy)
    {
        auto * self    = static_cast<WaylandEvent *>(data);
        self->m_mouseX = toPhysFloor(wl_fixed_to_int(sx));
        self->m_mouseY = toPhysFloor(wl_fixed_to_int(sy));

        Event event;
        event.type    = EventType::MouseMove;
        event.mouse.x = self->m_mouseX;
        event.mouse.y = self->m_mouseY;

        event.sourceWindow = self->m_focusSurface;

        self->queueEvent(event);
    }

    static void pointerButton(void * data,
                              wl_pointer * /*pointer*/,
                              uint32_t /*serial*/,
                              uint32_t time,
                              uint32_t button,
                              uint32_t state)
    {
        auto * self = static_cast<WaylandEvent *>(data);

        Event event;
        event.type    = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? EventType::MouseButtonPress
                                                                   : EventType::MouseButtonRelease;
        event.mouse.x = self->m_mouseX;
        event.mouse.y = self->m_mouseY;
        std::cout << "[WaylandEvent] Button " << button << " state=" << state << " at (" << event.mouse.x << ","
                  << event.mouse.y << ")" << std::endl;

        // Convert Linux button codes to MouseButton enum
        // BTN_LEFT=272, BTN_RIGHT=273, BTN_MIDDLE=274
        switch (button) {
        case 272: event.mouse.button = MouseButton::Left; break;
        case 273: event.mouse.button = MouseButton::Right; break;
        case 274: event.mouse.button = MouseButton::Middle; break;
        default: event.mouse.button = MouseButton::Left; break;
        }

        // Wayland has no native double-click signal; reconstruct via the
        // shared counter (time is ms since some reference epoch).
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
            event.mouse.clickCount = self->m_clickCounter.next(time, self->m_mouseX, self->m_mouseY, button);
        }

        event.sourceWindow = self->m_focusSurface;

        self->queueEvent(event);
    }

    static void pointerAxis(void * data, wl_pointer * /*pointer*/, uint32_t /*time*/, uint32_t axis, wl_fixed_t value)
    {
        auto * self = static_cast<WaylandEvent *>(data);

        // Only handle vertical scroll (axis 0)
        if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) {
            return;
        }

        Event event;
        event.type          = EventType::Scroll;
        event.mouse.x       = self->m_mouseX;
        event.mouse.y       = self->m_mouseY;
        event.scroll.deltaY = (value > 0) ? 1.0F : -1.0F;

        event.sourceWindow = self->m_focusSurface;

        self->queueEvent(event);
    }

    static void pointerFrame(void * /*data*/, wl_pointer * /*pointer*/)
    {
        // Events are complete for this frame
    }

    static void pointerAxisSource(void * /*data*/, wl_pointer * /*pointer*/, uint32_t /*source*/) { }

    static void pointerAxisStop(void * /*data*/, wl_pointer * /*pointer*/, uint32_t /*time*/, uint32_t /*axis*/) { }

    static void pointerAxisDiscrete(void * /*data*/, wl_pointer * /*pointer*/, uint32_t /*axis*/, int32_t /*discrete*/)
    {
    }

    static void pointerAxisValue120(void * /*data*/, wl_pointer * /*pointer*/, uint32_t /*axis*/, int32_t /*value120*/)
    {
    }

    static constexpr wl_pointer_listener s_pointerListener = { pointerEnter,       pointerLeave,    pointerMotion,
                                                               pointerButton,      pointerAxis,     pointerFrame,
                                                               pointerAxisSource,  pointerAxisStop, pointerAxisDiscrete,
                                                               pointerAxisValue120 };

    // -------- Keyboard listeners --------

    static void keyboardKeymap(void * data, wl_keyboard * /*keyboard*/, uint32_t format, int32_t fd, uint32_t size)
    {
        auto * self = static_cast<WaylandEvent *>(data);

        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }

        char * mapStr = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (mapStr == MAP_FAILED) {
            close(fd);
            return;
        }

        if (self->m_xkbKeymap) {
            xkb_keymap_unref(self->m_xkbKeymap);
        }
        if (self->m_xkbState) {
            xkb_state_unref(self->m_xkbState);
        }

        self->m_xkbKeymap = xkb_keymap_new_from_string(self->m_xkbContext,
                                                       mapStr,
                                                       XKB_KEYMAP_FORMAT_TEXT_V1,
                                                       XKB_KEYMAP_COMPILE_NO_FLAGS);

        munmap(mapStr, size);
        close(fd);

        if (self->m_xkbKeymap) {
            self->m_xkbState = xkb_state_new(self->m_xkbKeymap);
        }
    }

    static void keyboardEnter(void * data,
                              wl_keyboard * /*keyboard*/,
                              uint32_t /*serial*/,
                              wl_surface * surface,
                              wl_array * /*keys*/)
    {
        auto * self         = static_cast<WaylandEvent *>(data);
        self->m_mainSurface = surface;
    }

    static void keyboardLeave(void * data, wl_keyboard * /*keyboard*/, uint32_t /*serial*/, wl_surface * /*surface*/)
    {
        auto * self         = static_cast<WaylandEvent *>(data);
        self->m_mainSurface = nullptr;
    }

    static void keyboardKey(void * data,
                            wl_keyboard * /*keyboard*/,
                            uint32_t /*serial*/,
                            uint32_t /*time*/,
                            uint32_t key,
                            uint32_t state)
    {
        auto * self = static_cast<WaylandEvent *>(data);

        if (!self->m_xkbState) {
            return;
        }

        Event event;
        event.type = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? EventType::KeyPress : EventType::KeyRelease;

        // Convert evdev keycode to XKB keycode (add 8)
        xkb_keycode_t keycode = key + 8;
        xkb_keysym_t  keysym  = xkb_state_key_get_one_sym(self->m_xkbState, keycode);
        event.key.keysym      = keysym;

        // Get modifiers
        event.key.modifiers = self->modifiers();

        // Get text representation and logical key name for key press
        if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            xkb_state_key_get_utf8(self->m_xkbState, keycode, event.key.text.data(), event.key.text.size());
            xkb_keysym_get_name(keysym, event.key.name.data(), event.key.name.size());
        }

        self->queueEvent(event);
    }

    static void keyboardModifiers(void * data,
                                  wl_keyboard * /*keyboard*/,
                                  uint32_t /*serial*/,
                                  uint32_t modsDepressed,
                                  uint32_t modsLatched,
                                  uint32_t modsLocked,
                                  uint32_t group)
    {
        auto * self = static_cast<WaylandEvent *>(data);
        if (self->m_xkbState) {
            xkb_state_update_mask(self->m_xkbState, modsDepressed, modsLatched, modsLocked, 0, 0, group);
        }
    }

    static void keyboardRepeatInfo(void * /*data*/, wl_keyboard * /*keyboard*/, int32_t /*rate*/, int32_t /*delay*/)
    {
        // Could implement key repeat here
    }

    static constexpr wl_keyboard_listener s_keyboardListener = {
        keyboardKeymap, keyboardEnter, keyboardLeave, keyboardKey, keyboardModifiers, keyboardRepeatInfo
    };

    KeyModifier modifiers() const
    {
        if (!m_xkbState) {
            return KeyModifier::None;
        }

        KeyModifier mods = KeyModifier::None;

        if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE)) {
            mods = mods | KeyModifier::Shift;
        }
        if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE)) {
            mods = mods | KeyModifier::Control;
        }
        if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE)) {
            mods = mods | KeyModifier::Alt;
        }
        if (xkb_state_mod_name_is_active(m_xkbState, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE)) {
            mods = mods | KeyModifier::Meta;
        }

        return mods;
    }
};

} // namespace Ui::Window::Platform

#endif // HAVE_WAYLAND
