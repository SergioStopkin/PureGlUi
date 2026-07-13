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

#include "common/unicode.h"
#include "ui/interface/ieventos.h"
#include "ui/interface/iwindow.h"
#include "ui/window/clickcounter.h"
#include "ui/window/event.h"
#include "ui/window/platform/win32window.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

namespace Ui::Window::Platform {

// Flip to true to trace Win32 mouse/key messages and capture state.
// Useful when behavior diverges from X11/Wayland (e.g. drags releasing early).
constexpr bool WIN32_EVENT_DEBUG = true;

/**
 * @brief Win32 event handling implementation
 *
 * Implements Ui::IEventOS for Windows platform.
 * Does NOT own any windows - receives them as parameters.
 * Handles Windows messages conversion to platform-agnostic Event struct.
 */
class Win32Event final : public Ui::IEventOS {
public:
    Win32Event()           = default;
    ~Win32Event() override = default;

    /**
     * @brief Initialize (no-op for Win32)
     */
    void init(Ui::IWindow & /*window*/) override { }

    /**
     * @brief Register a child window for event routing
     * @param handle Native window handle
     * @param id Child window ID
     */
    void registerChildWindow(id_t id, NativeWindowHandle handle) override
    {
        (void)id;
        (void)handle;
        // TODO(sergio): Implement child window registration
    }

    /**
     * @brief Unregister a child window
     * @param handle Native window handle
     */
    void unregisterChildWindow(NativeWindowHandle handle) override
    {
        (void)handle;
        // TODO(sergio): Implement child window unregistration
    }

    void addPopupWindow(NativeWindowHandle /*handle*/) override { }
    void removePopupWindow(NativeWindowHandle /*handle*/) override { }

    // -------- Ui::IEventOS implementation --------

    [[nodiscard]] bool hasPendingEvents(Ui::IWindow & window) const override
    {
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE) != 0) {
            return true;
        }
        // WndProc updates window bound directly during the modal resize/move
        // loop. Detect the change here so the event loop keeps spinning.
        const auto & bound = window.bound();
        return (bound.w != m_lastWidth || bound.h != m_lastHeight || bound.x != m_lastX || bound.y != m_lastY);
    }

    bool pollEvent(Event & event, Ui::IWindow & window) override
    {
        // Surface bound changes before draining the message queue. WM_SIZE
        // is SENT (not posted) from inside DefWindowProc for maximize/restore
        // and from the modal resize loop - our wndProc updates m_bound
        // directly and then control unwinds through another DispatchMessage
        // call. If we only synthesize Resize when PeekMessage returns 0, a
        // continuous stream of messages (WM_PAINT, WM_MOUSEMOVE from a content-surface
        // child drag, etc.) starves the synth path and the Resize event
        // never fires - leaving content surface sized for the old client rect.
        // Position changes ride the same event: a move-only drag emits Resize
        // with the UNCHANGED size - the shell handler persists the new frame
        // position and its same-size early-out skips the resize cascade. This
        // matches how X11's ConfigureNotify behaves on a move. (Wayland has no
        // window position at all, so only this backend tracks x/y.)
        const auto & bound = window.bound();
        if (bound.w != m_lastWidth || bound.h != m_lastHeight || bound.x != m_lastX || bound.y != m_lastY) {
            m_lastWidth         = bound.w;
            m_lastHeight        = bound.h;
            m_lastX             = bound.x;
            m_lastY             = bound.y;
            event.type          = EventType::Resize;
            event.resize.width  = bound.w;
            event.resize.height = bound.h;
            return true;
        }

        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            event = convertWin32Event(msg);
            return true;
        }

        return false;
    }

    void flush(Ui::IWindow & window) override
    {
        auto hwnd = window.nativeHandle();
        if (hwnd) {
            UpdateWindow(hwnd);
        }
    }

    void setDoubleClickConfig(uint32_t intervalMs, int distancePx) override
    {
        m_clickCounter.configure(intervalMs, distancePx);
    }

    bool copyToClipboard(Ui::IWindow & window, const std::string & text) override
    {
        auto hwnd = window.nativeHandle();
        if (hwnd == nullptr || OpenClipboard(hwnd) == 0) {
            return false;
        }

        EmptyClipboard();

        // CF_UNICODETEXT: convert UTF-8 to UTF-16, store in GlobalAlloc'd buffer (NUL-terminated)
        const std::wstring wide = Common::Unicode::fromUtf8(text);
        bool               ok   = false;
        HGLOBAL            hMem = GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
        if (hMem != nullptr) {
            auto * dst = static_cast<wchar_t *>(GlobalLock(hMem));
            if (dst != nullptr) {
                std::copy_n(wide.data(), wide.size(), dst);
                dst[wide.size()] = L'\0';
                GlobalUnlock(hMem);
                // Ownership of hMem transfers to the clipboard on success
                if (SetClipboardData(CF_UNICODETEXT, hMem) != nullptr) {
                    ok = true;
                } else {
                    GlobalFree(hMem);
                }
            } else {
                GlobalFree(hMem);
            }
        }

        CloseClipboard();
        return ok;
    }

private:
    /**
     * @brief Convert Win32 message to platform-agnostic event
     */
    Event convertWin32Event(const MSG & msg)
    {
        Event event;
        event.sourceWindow = msg.hwnd;

        if constexpr (WIN32_EVENT_DEBUG) {
            const bool isMouseMsg = (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP
                                     || msg.message == WM_RBUTTONDOWN || msg.message == WM_RBUTTONUP
                                     || msg.message == WM_MBUTTONDOWN || msg.message == WM_MBUTTONUP
                                     || msg.message == WM_MOUSEMOVE || msg.message == WM_MOUSEWHEEL
                                     || msg.message == WM_CAPTURECHANGED);
            if (isMouseMsg) {
                std::cout << "[Win32Event] msg=0x" << std::hex << msg.message << std::dec << " hwnd=" << msg.hwnd
                          << " x=" << static_cast<short>(LOWORD(msg.lParam))
                          << " y=" << static_cast<short>(HIWORD(msg.lParam)) << " captured=" << GetCapture()
                          << " buttons=0x" << std::hex << m_pressedButtons << std::dec << std::endl;
            }
        }

        switch (msg.message) {
        case WM_QUIT:
        case Win32Window::WM_APP_CLOSE: event.type = EventType::CloseRequested; break;

        case WM_LBUTTONDOWN:
            event.type             = EventType::MouseButtonPress;
            event.mouse.x          = LOWORD(msg.lParam);
            event.mouse.y          = HIWORD(msg.lParam);
            event.mouse.button     = MouseButton::Left;
            event.mouse.clickCount = m_clickCounter.next(msg.time, event.mouse.x, event.mouse.y, WM_LBUTTONDOWN);
            acquireCapture(msg.hwnd, 0x1U);
            break;

        case WM_LBUTTONUP:
            event.type         = EventType::MouseButtonRelease;
            event.mouse.x      = LOWORD(msg.lParam);
            event.mouse.y      = HIWORD(msg.lParam);
            event.mouse.button = MouseButton::Left;
            releaseCapture(0x1U);
            break;

        case WM_RBUTTONDOWN:
            event.type             = EventType::MouseButtonPress;
            event.mouse.x          = LOWORD(msg.lParam);
            event.mouse.y          = HIWORD(msg.lParam);
            event.mouse.button     = MouseButton::Right;
            event.mouse.clickCount = m_clickCounter.next(msg.time, event.mouse.x, event.mouse.y, WM_RBUTTONDOWN);
            acquireCapture(msg.hwnd, 0x2U);
            break;

        case WM_RBUTTONUP:
            event.type         = EventType::MouseButtonRelease;
            event.mouse.x      = LOWORD(msg.lParam);
            event.mouse.y      = HIWORD(msg.lParam);
            event.mouse.button = MouseButton::Right;
            releaseCapture(0x2U);
            break;

        case WM_MBUTTONDOWN:
            event.type             = EventType::MouseButtonPress;
            event.mouse.x          = LOWORD(msg.lParam);
            event.mouse.y          = HIWORD(msg.lParam);
            event.mouse.button     = MouseButton::Middle;
            event.mouse.clickCount = m_clickCounter.next(msg.time, event.mouse.x, event.mouse.y, WM_MBUTTONDOWN);
            acquireCapture(msg.hwnd, 0x4U);
            break;

        case WM_MBUTTONUP:
            event.type         = EventType::MouseButtonRelease;
            event.mouse.x      = LOWORD(msg.lParam);
            event.mouse.y      = HIWORD(msg.lParam);
            event.mouse.button = MouseButton::Middle;
            releaseCapture(0x4U);
            break;

        case WM_MOUSEWHEEL: {
            event.type = EventType::Scroll;
            // WM_MOUSEWHEEL's lParam holds SCREEN coordinates (unlike WM_MOUSEMOVE
            // / WM_LBUTTONDOWN which use client coords). Convert to client so
            // hitTestChild() routes the scroll to the content surface - otherwise
            // scrolls near the client edge land outside the content surface's client rect,
            // get routed to UiRenderer::onScroll (a no-op), and zoom ticks
            // are silently dropped. Coords are signed short (multi-monitor
            // setups can produce negative values).
            POINT pt = { static_cast<short>(LOWORD(msg.lParam)), static_cast<short>(HIWORD(msg.lParam)) };
            ScreenToClient(msg.hwnd, &pt);
            event.mouse.x = pt.x;
            event.mouse.y = pt.y;
            // Preserve wheel-tick magnitude. Windows coalesces rapid wheel rotations
            // into a single WM_MOUSEWHEEL with delta = N * WHEEL_DELTA (e.g. 360 for
            // three notches rolled quickly). X11 sends one ButtonPress per notch, so
            // clamping to +-1.0 here makes Windows zoom feel much slower than Linux.
            // Sign matches X11/Wayland: positive deltaY = wheel toward user = zoom out.
            const int delta     = GET_WHEEL_DELTA_WPARAM(msg.wParam);
            event.scroll.deltaY = -static_cast<fpx_t>(delta) / static_cast<fpx_t>(WHEEL_DELTA);
        } break;

        case WM_MOUSEMOVE:
            event.type    = EventType::MouseMove;
            event.mouse.x = LOWORD(msg.lParam);
            event.mouse.y = HIWORD(msg.lParam);
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            event.type                = EventType::KeyPress;
            event.key.keysym          = static_cast<uint32_t>(msg.wParam);
            event.key.modifiers       = convertWin32Modifiers();
            const std::string keyname = vkToKeyName(msg.wParam);
            std::strncpy(event.key.name.data(), keyname.c_str(), event.key.name.size() - 1);
        } break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            event.type          = EventType::KeyRelease;
            event.key.keysym    = static_cast<uint32_t>(msg.wParam);
            event.key.modifiers = convertWin32Modifiers();
            break;

        default: break;
        }

        return event;
    }

    /**
     * @brief Convert Win32 virtual-key code to logical key name matching X11
     * XKeysymToString output. Letters return lowercase ("q"), F-keys "F1"..
     * "F24", and special keys use X11 names ("Return", "Escape", "Left", ...).
     */
    static std::string vkToKeyName(WPARAM vk)
    {
        if (vk >= 'A' && vk <= 'Z') {
            return std::string(1, static_cast<char>(vk + ('a' - 'A')));
        }
        if (vk >= '0' && vk <= '9') {
            return std::string(1, static_cast<char>(vk));
        }
        if (vk >= VK_F1 && vk <= VK_F24) {
            return "F" + std::to_string(vk - VK_F1 + 1);
        }
        // clang-format off
        switch (vk) {
        case VK_BACK:   return "BackSpace";
        case VK_TAB:    return "Tab";
        case VK_RETURN: return "Return";
        case VK_ESCAPE: return "Escape";
        case VK_SPACE:  return "space";
        case VK_PRIOR:  return "Prior";
        case VK_NEXT:   return "Next";
        case VK_END:    return "End";
        case VK_HOME:   return "Home";
        case VK_LEFT:   return "Left";
        case VK_UP:     return "Up";
        case VK_RIGHT:  return "Right";
        case VK_DOWN:   return "Down";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        default:        return {};
        }
        // clang-format on
    }

    /**
     * @brief Convert Win32 modifier state to platform-agnostic modifiers
     */
    static KeyModifier convertWin32Modifiers()
    {
        KeyModifier mods = KeyModifier::None;

        if (GetKeyState(VK_SHIFT) & 0x8000) {
            mods = mods | KeyModifier::Shift;
        }
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            mods = mods | KeyModifier::Control;
        }
        if (GetKeyState(VK_MENU) & 0x8000) { // Alt
            mods = mods | KeyModifier::Alt;
        }

        return mods;
    }

    // Pointer-grab parity with X11/Wayland. Those servers implicitly grab the
    // pointer for the duration of a press, so motion keeps flowing to the
    // pressed window even when the cursor exits its client rect. Win32 has no
    // such grab - without SetCapture, dragging out of the content surface stops
    // delivering WM_MOUSEMOVE and the rotation freezes mid-gesture. Track a
    // small bitmask of held buttons so capture spans chord presses too.
    void acquireCapture(HWND hwnd, unsigned button)
    {
        if (hwnd == nullptr) {
            return;
        }
        if (m_pressedButtons == 0U) {
            HWND prev = SetCapture(hwnd);
            if constexpr (WIN32_EVENT_DEBUG) {
                std::cout << "[Win32Event] SetCapture hwnd=" << hwnd << " prev=" << prev << std::endl;
            }
        }
        m_pressedButtons |= button;
    }

    void releaseCapture(unsigned button)
    {
        m_pressedButtons &= ~button;
        if (m_pressedButtons == 0U) {
            ReleaseCapture();
            if constexpr (WIN32_EVENT_DEBUG) {
                std::cout << "[Win32Event] ReleaseCapture" << std::endl;
            }
        }
    }

    // Last known window bound for detecting WndProc-driven resize/move
    fpx_t m_lastWidth  = 0;
    fpx_t m_lastHeight = 0;
    fpx_t m_lastX      = 0;
    fpx_t m_lastY      = 0;

    // Multi-click burst counter. We don't enable CS_DBLCLKS on the window
    // class, so WM_*BUTTONDOWN arrives even for the second press; the
    // counter classifies it the same way X11/Wayland do.
    ClickCounter m_clickCounter;

    unsigned m_pressedButtons = 0U; // L=0x1, R=0x2, M=0x4
};

} // namespace Ui::Window::Platform
