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

#include "common/noncopyable.h"
#include "ui/config.h"
#include "ui/render/elementevent.h"
#include "ui/render/uielement.h"
#include "ui/res/resmanager.h"
#include "ui/res/store/menuheight.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/menu.h"
#include "ui/res/util.h"
#include "ui/shelldebug.h"
#include "ui/type.h"
#include "ui/window/event.h"
#include "ui/window/mousebutton.h"
#include "ui/window/mouseevent.h"
#include "ui/window/windowmanager.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ui {

// The open popup menu - a top-bar dropdown or a dock row's - and how the pointer works it,
// as every OS's menu bar does (doc/api/shell-actions.md "Menu bar behaviour"): a title's
// press opens its list, a move onto another title switches to it, a press beside the list
// closes it, and the list follows the main window when it moves or resizes. Its windows
// are the WindowManager's; a click it does not decide itself is the shell's to dispatch
class MenuBar final : private Common::NonCopyable {
public:
    MenuBar(Ui::Res::ResManager & resManager, Ui::Window::WindowManager & windowManager)
        : m_resManager(resManager)
        , m_windowManager(windowManager)
    {
    }

    [[nodiscard]] bool isOpen() const { return m_openMenuId != Ui::INVALID_ID; }

    // The open menu's id, INVALID_ID with none
    [[nodiscard]] id_t openId() const { return m_openMenuId; }

    // Whether a hover's switch to another list waits for its frame
    [[nodiscard]] bool isSwitchPending() const { return m_menuSwitchPending; }

    // A top-bar dropdown, hung under its menu button
    void open(id_t menuId)
    {
        // Under its button where the layout put it - empty before the first layout, the
        // window's top-left then
        const Ui::Res::Type::bound_t & button = m_windowManager.elementBound(menuId);

        const bool isMenu = m_resManager.visitMenuItem(menuId, [this, &button](const Ui::Res::Type::menu_t & menu) {
            if constexpr (SHELL_DEBUG) {
                std::cout << "[Shell] Popup position: menuId=" << menu.id << " X=" << button.x
                          << " Y=" << button.y + button.h << std::endl;
            }
            show(menu, button.x, button.y + button.h);
        });
        if (!isMenu) {
            std::cerr << "[Shell] Menu not found: " << menuId << std::endl;
        }
    }

    // The list closed once the current event drains: the click may come from inside the
    // popup window about to go
    void closeAfterEvent()
    {
        trace("closed", m_openMenuId);
        m_openMenuId = Ui::INVALID_ID;
        m_resManager.clearActiveMenu();
        m_windowManager.requestContentRefresh();
        m_windowManager.subscribe().defer([this]() { destroy(); });
    }

    // A dock Menu row's menu, right-aligned under the row where its value and chevron sit
    void openRowMenu(id_t elementId)
    {
        // The press that started this click already closed this row's menu, so the
        // click IS the close - opening again would leave the chevron unable to close
        if (m_pressClosedRowMenu == elementId) {
            m_pressClosedRowMenu = Ui::INVALID_ID;
            return;
        }
        const Ui::Res::Type::menu_t menu = m_resManager.findMenuItemByKey(m_windowManager.rowMenuKey(elementId));
        if (menu.id == Ui::INVALID_ID || menu.items.empty()) {
            std::cerr << "[Shell] Dock row names no menu with items: element " << elementId << std::endl;
            return;
        }

        // Above the row when the list would run past the bottom of the window, which is
        // where a dock's last rows are. The row element's bound is the whole row: its
        // value column shares the id and is laid out after it
        const Ui::Res::Type::bound_t & row     = m_windowManager.elementBound(elementId);
        const fpx_t                    height  = cssHeightOf(menu);
        const fpx_t                    below   = row.y + row.h;
        const bool                     isBelow = below + height <= toCss(m_windowManager.windowHeight());
        const fpx_t x = std::max(0.0F, row.x + row.w - m_windowManager.menus().cssWidthOf(menu.items));
        show(menu, x, isBelow ? below : row.y - height);
        m_rowMenuElement = elementId;
    }

    // A dialog opens over the bar: the hover it remembered is gone with the pointer
    void forgetHover() { m_hoveredMenuId = Ui::INVALID_ID; }

    // Close any open list (no-op if none); the host calls it before opening a native
    // modal (e.g. the file dialog)
    void close()
    {
        if (isOpen()) {
            destroy();
        }
    }

    // The list destroyed and the menu state cleared; `syncDisplay` makes sure the window
    // is off the screen before returning
    void destroy(bool syncDisplay = false)
    {
        trace("list destroyed", m_openMenuId);
        m_windowManager.menus().close(syncDisplay);

        m_openMenuId     = Ui::INVALID_ID;
        m_rowMenuElement = Ui::INVALID_ID;
        m_resManager.clearActiveMenu();
        m_windowManager.requestContentRefresh();
        // Last, once no menu is open: a hover then switches to no other
        m_windowManager.rehover();
    }

    // Hierarchical key of the deepest active menu node: the highlighted item
    // (across popup + submenu) if any, else the open dropdown. "" = nothing open.
    [[nodiscard]] Ui::key_t activeKey() const
    {
        // A row menu has no key path to come back by - its anchor is a dock row that
        // a reload or a resize may have moved or removed - so it is never restored
        if (m_openMenuId == Ui::INVALID_ID || m_rowMenuElement != Ui::INVALID_ID) {
            return {};
        }
        const id_t activeItem = m_windowManager.menus().activeItemId();
        const id_t node       = (activeItem != Ui::INVALID_ID) ? activeItem : m_openMenuId;
        return m_resManager.findMenuItem(node).key;
    }

    // Reopen the dropdown + any submenu named by a hierarchical key ("File",
    // "View:Theme", "View:Theme:default") and re-highlight the leaf node.
    void restore(const Ui::key_t & activeKey)
    {
        const std::vector<Ui::key_t> segments = Ui::Res::Util::splitMenuKey(activeKey);
        if (segments.empty()) {
            return;
        }

        const Ui::Res::Type::menu_t dropdown = m_resManager.findMenuItemByKey(segments.front());
        if (dropdown.id == Ui::INVALID_ID) {
            return;
        }
        open(dropdown.id);

        // A node two levels deep that has children is the submenu to reopen
        // (covers both a hovered submenu-parent and a hovered submenu-child).
        if (segments.size() >= 2) {
            const Ui::key_t             parentKey = segments.at(0) + ":" + segments.at(1);
            const Ui::Res::Type::menu_t parent    = m_resManager.findMenuItemByKey(parentKey);
            if (parent.id != Ui::INVALID_ID && !parent.items.empty()) {
                m_windowManager.menus().reopenSubmenu(parent.id);
            }
        }

        const Ui::Res::Type::menu_t leaf = m_resManager.findMenuItemByKey(activeKey);
        if (leaf.id != Ui::INVALID_ID) {
            m_windowManager.menus().setActiveItem(leaf.id);
        }
    }

    // Recreate the open popup (+ submenu + active-item highlight) at its anchored
    // position - e.g. after a window move/resize relocates or rebuilds the popup
    // window. Mirrors the menu-state preservation a reload does, so the highlighted
    // item survives all three.
    void reopen()
    {
        // Closed rather than left behind at the old screen position
        if (m_rowMenuElement != Ui::INVALID_ID) {
            destroy();
            return;
        }
        const Ui::key_t key = activeKey();
        if (key.empty()) {
            return;
        }
        destroy();
        restore(key);
    }

    // The main window resized: the list recreated at its new anchor - but only a
    // root-parented popup, since a child popup is re-anchored by the server and
    // recreating it would blink
    void reopenAfterResize()
    {
        if (isOpen() && m_windowManager.menus().isOpen() && !m_windowManager.menus().followsParent()) {
            reopen();
        }
    }

    // The main window resizing: the list hidden until reopenAfterResize
    void hideForResize()
    {
        if (isOpen()) {
            m_windowManager.menus().hideForResize();
        }
    }

    // Another app took the focus: an open list closes, as every OS's own menu does, and a
    // switch waiting to make one stops
    void closeForFocusLoss()
    {
        m_menuSwitchPending = false;
        if (m_windowManager.menus().isOpen()) {
            destroy();
        }
    }

    // Track the popup while the main window moves: shift it by the window delta, hide
    // during the move, recreate once the move settles
    void followWindowMove()
    {
        if (m_openMenuId == Ui::INVALID_ID || !m_windowManager.menus().isOpen()) {
            return;
        }
        // An X11 child popup is moved by the server with its parent; tracking it
        // here would double-move it. Only root-parented popups need manual moves.
        if (m_windowManager.menus().followsParent()) {
            return;
        }
        int currentX = 0;
        int currentY = 0;
        m_windowManager.mainWindowScreenPosition(currentX, currentY);
        if (currentX == m_lastWindowX && currentY == m_lastWindowY) {
            return;
        }
        const int deltaX = currentX - m_lastWindowX;
        const int deltaY = currentY - m_lastWindowY;
        m_lastWindowX    = currentX;
        m_lastWindowY    = currentY;

        // The popup (+ submenu) moved by the same delta as the main window so they stay
        // attached - no destroy/recreate
        m_windowManager.menus().moveBy(deltaX, deltaY);
    }

    // The pointer onto menu button `menuId`: a switch to its list when one is open
    void hover(id_t menuId)
    {
        // The newest hover is the one owed: one held off before it is dropped
        m_heldMenuId = Ui::INVALID_ID;

        // One read of the menu where it is: an unknown id reads as an enabled menu with no
        // action, as an empty one does
        bool isActionMenu = false;
        bool isEnabled    = true;
        m_resManager.visitMenuItem(menuId, [&isActionMenu, &isEnabled](const Ui::Res::Type::menu_t & menu) {
            isActionMenu = !menu.actionKey.empty();
            isEnabled    = menu.enabled;
        });

        // Skip action menus (icon buttons with no popup)
        if (isActionMenu) {
            trace("skipped, an action button", menuId);
            return;
        }

        // A disabled top menu has no usable dropdown: skip it. Leave the menu
        // state untouched - moving across a disabled menu keeps the currently-open
        // popup open (so moving back to it finds it still open) and never opens a
        // dropdown of its own.
        if (!isEnabled) {
            trace("skipped, disabled", menuId);
            return;
        }

        // If no menu is currently open, hovering should NOT open a popup --
        // a click is required to open the first menu. Just remember hovered id.
        if (m_openMenuId == Ui::INVALID_ID && !m_menuSwitchPending) {
            if (m_hoveredMenuId != menuId) {
                if constexpr (SHELL_DEBUG) {
                    std::cout << "[Shell] Hovered menu (no popup open): " << menuId << std::endl;
                }
                m_hoveredMenuId = menuId;
            }
            trace("no list open, nothing to switch", menuId);
            return;
        }

        // Don't switch to the same menu
        if (m_openMenuId == menuId) {
            trace("already open", menuId);
            return;
        }

        // A dock row's menu is not part of the bar: sweeping the pointer across the
        // bar must not trade it for one of the bar's dropdowns
        if (m_rowMenuElement != Ui::INVALID_ID) {
            trace("skipped, a row's menu is open", menuId);
            return;
        }

        // Don't switch immediately after activation -- wait one render pass
        // to avoid destroying a popup that was just created this frame. Nor while a
        // switch waits for its frame. Either way the hover is held, and replayHeldHover
        // asks again once nothing holds it: a pointer resting on the button sends no more
        if (m_menuJustActivated || m_menuSwitchPending) {
            m_heldMenuId = menuId;
            trace("held", menuId);
            return;
        }
        trace("switch started", menuId);

        // Always defer menu switching when hovering:
        // 1. Destroy old popup now (so the area it covered can be redrawn)
        // 2. Mark main window for re-render
        // 3. Create new popup AFTER render (via deferred mechanism)
        // This ensures corner capture sees the properly rendered background.
        destroy(true); // Destroy old popup, triggers requestContentRefresh
        m_menuSwitchPending = true;
        m_windowManager.subscribe().defer([this, menuId]() {
            // Stopped while it waited: the app lost the focus
            if (!m_menuSwitchPending) {
                trace("switch stopped", menuId);
                return;
            }
            if constexpr (SHELL_DEBUG) {
                std::cout << "[Shell] Committing deferred menu switch to: " << menuId << std::endl;
            }
            open(menuId);
            m_menuSwitchPending = false;
            m_menuJustActivated = true;
            trace("switch made its list", menuId);
        });
    }

    // A main-window move to `x`, `y` physical px: onto a menu button not hovered yet, its hover
    void hoverAt(int x, int y)
    {
        if (!m_windowManager.hasUiRenderer()) {
            return;
        }
        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);
        for (const auto & m : m_resManager.menus()) {
            const auto & b = m_windowManager.elementBound(m.id);
            if (b.w > 0 && b.contains(cssX, cssY)) {
                if (m_hoveredMenuId != m.id) {
                    trace("move onto a button", m.id);
                    m_hoveredMenuId = m.id;
                    hover(m.id);
                }
                break;
            }
        }
    }

    // A hover hover() held, once nothing holds a switch off, if the pointer is still on
    // that button: resting there it sends nothing more, so it is asked again
    void replayHeldHover()
    {
        if (m_heldMenuId == Ui::INVALID_ID || m_menuJustActivated || m_menuSwitchPending) {
            return;
        }
        const id_t menuId = std::exchange(m_heldMenuId, Ui::INVALID_ID);
        if (m_windowManager.isPointerIn(m_windowManager.elementBound(menuId))) {
            trace("replaying a held hover", menuId);
            hover(menuId);
        } else {
            trace("dropped a held hover, the pointer moved off", menuId);
        }
    }

    // A list just made waits for one pass or frame before a hover may switch it: `why`
    // that wait is over
    void endWait(std::string_view why)
    {
        if (m_menuJustActivated) {
            m_menuJustActivated = false;
            trace(why, m_openMenuId);
        }
    }

    // A new press: the row menu an earlier press closed is forgotten
    void forgetClosingPress() { m_pressClosedRowMenu = Ui::INVALID_ID; }

    // A press on the list's window the list did not take - outside its visual region: on a
    // menu-bar title it switches, else the list closes and the press goes on to the main
    // window there
    void pressOnList(const Ui::Window::Event & event)
    {
        if (m_windowManager.popupEventConsumed()) {
            return;
        }
        bool shouldClosePopup = true;

        const auto [mainX, mainY] = m_windowManager.menus().mainPointOf(event.mouse.x, event.mouse.y);

        if (m_windowManager.hasUiRenderer()) {
            const int menuBarHeight = toPhysFloor(m_resManager.layout().topMenu.height);
            if (mainY >= 0 && mainY < menuBarHeight) {
                if (mainX < menusTotalWidthPhysical()) {
                    const bool hitMenuItem = pressMainAt(mainX, mainY, event.mouse).isHit();
                    m_windowManager.requestMainRender();
                    if (hitMenuItem) {
                        shouldClosePopup = false;
                    }
                    if constexpr (SHELL_DEBUG) {
                        std::cout << (hitMenuItem ? "[Shell] Clicked menu item, switching popup"
                                                  : "[Shell] Clicked menu bar between buttons - closing "
                                                    "popup")
                                  << std::endl;
                    }
                } else {
                    if constexpr (SHELL_DEBUG) {
                        std::cout << "[Shell] Clicked menu bar empty space after menus (x=" << mainX << " > "
                                  << menusTotalWidthPhysical() << ") - closing popup" << std::endl;
                    }
                }
            } else {
                if (m_windowManager.menus().contains(event.mouse.x, event.mouse.y)) {
                    shouldClosePopup = false;
                }
            }
        }

        if (shouldClosePopup) {
            if constexpr (SHELL_DEBUG) {
                std::cout << "[Shell] Closing popup (clicked outside menu and popup area)" << std::endl;
            }
            m_pressClosedRowMenu = rowMenuUnder(mainX, mainY);
            destroy();
            // Forward click to main window
            if (event.mouse.button == Ui::Window::MouseButton::Left) {
                pressMainAt(mainX, mainY, event.mouse);
                m_windowManager.requestMainRender();
            }
        }
    }

    // A press on the main window or a content surface while the list is open: the list
    // closes, unless on a menu-bar title, whose press decides
    void pressBesideList(const Ui::Window::Event & event)
    {
        if (!m_windowManager.menus().isOpen()) {
            return;
        }
        bool      closePopup    = true;
        const int menuBarHeight = toPhysFloor(m_resManager.layout().topMenu.height);
        if (event.mouse.y >= 0 && event.mouse.y < menuBarHeight) {
            if (event.mouse.x < menusTotalWidthPhysical()) {
                if constexpr (SHELL_DEBUG) {
                    std::cout << "[Shell] Press on the menu bar while popup open - its title decides below"
                              << std::endl;
                }
                closePopup = false;
            } else {
                if constexpr (SHELL_DEBUG) {
                    std::cout << "[Shell] Click in menu bar empty space (after menus at x=" << event.mouse.x << " > "
                              << menusTotalWidthPhysical() << ") - closing popup" << std::endl;
                }
            }
        } else {
            if constexpr (SHELL_DEBUG) {
                std::cout << "[Shell] Closing popup (clicked outside)" << std::endl;
            }
        }
        if (closePopup) {
            m_pressClosedRowMenu = rowMenuUnder(event.mouse.x, event.mouse.y);
            destroy();
        }
    }

    // A move over the list's window that stands over the menu bar: the bar's hover follows,
    // so a move onto another title switches to it
    void moveOverBarFromList(const Ui::Window::Event & event)
    {
        if (!m_windowManager.hasUiRenderer() || !m_windowManager.menus().isOpen()) {
            return;
        }
        const auto [mainX, mainY] = m_windowManager.menus().mainPointOf(event.mouse.x, event.mouse.y);

        const int menuBarHeight = toPhysFloor(m_resManager.layout().topMenu.height);
        if (mainY >= 0 && mainY < menuBarHeight) {
            const bool isHoverChanged = m_windowManager.onMouseMove(mainX, mainY);
            if constexpr (MENU_HOVER_DEBUG) {
                std::cout << "[MenuHover] move over the bar from the list at " << mainX << "," << mainY
                          << " hover changed=" << isHoverChanged << std::endl;
            }
            if (isHoverChanged) {
                m_windowManager.requestMainRender();
            }
        }
    }

    // A menu-bar title that opens a list, not an action button (actionKey): it reacts to the
    // press, as every OS's menu bar does - doc/api/shell-actions.md "Menu bar behaviour" -
    // and its release does nothing. Whatever else is pressed acts on the release
    [[nodiscard]] bool isListTitle(const Ui::Render::element_event_t & event) const
    {
        return event.type == Ui::Render::UiElementType::MenuButton && event.id != Ui::INVALID_ID
            && m_resManager.actionKeyFor(event.id).empty();
    }

    // One line of the menu-hover trace: what happened, to which menu, and the state that
    // decides the next hover
    void trace(std::string_view what, id_t menuId) const
    {
        if constexpr (MENU_HOVER_DEBUG) {
            const auto nameOf = [this](id_t id) {
                return id == Ui::INVALID_ID ? std::string { "-" } : m_resManager.findMenuItem(id).label;
            };
            std::cout << "[MenuHover] " << what << ": menu=" << nameOf(menuId) << " open=" << nameOf(m_openMenuId)
                      << " hovered=" << nameOf(m_hoveredMenuId) << " held=" << nameOf(m_heldMenuId)
                      << " pending=" << m_menuSwitchPending << " justActivated=" << m_menuJustActivated << std::endl;
        }
    }

private:
    Ui::Res::ResManager &       m_resManager;
    Ui::Window::WindowManager & m_windowManager;

    id_t m_openMenuId = Ui::INVALID_ID; // ID of currently open popup menu (INVALID = none)
    // The dock Menu row whose menu is open; INVALID when the open popup, if any,
    // belongs to the top bar. It decides how a move, a reload and a hover treat it
    id_t m_rowMenuElement = Ui::INVALID_ID;
    // A press on that same row just closed its menu, so the click the press ends is
    // the close rather than a request to open again. Cleared by the next press
    id_t m_pressClosedRowMenu = Ui::INVALID_ID;
    id_t m_hoveredMenuId      = Ui::INVALID_ID; // ID of menu currently hovered (avoids duplicate hover actions)
    id_t m_heldMenuId         = Ui::INVALID_ID; // a hover hover() held off, for replayHeldHover

    // Where the main window stood, for followWindowMove
    int m_lastWindowX = 0;
    int m_lastWindowY = 0;

    bool m_menuSwitchPending = false; // Re-entry guard for deferred menu switch; a focus loss clears it to stop one
    bool m_menuJustActivated = false; // True briefly after creating a popup until it renders once

    // Everything a popup menu needs once its anchor is known, for a top-bar dropdown
    // and a dock row's menu alike: the anchor is the popup's top-left corner in
    // main-window CSS px, which is where the two differ and all they differ in
    void show(const Ui::Res::Type::menu_t & menu, fpx_t anchorXCss, fpx_t anchorYCss)
    {
        // Destroy any existing popup before creating a new one
        // Use syncDisplay=true to ensure old popup is removed before corner capture
        if (m_openMenuId != Ui::INVALID_ID) {
            destroy(true);
        }

        m_openMenuId = menu.id;
        m_resManager.setActiveMenu(menu.id);
        m_windowManager.requestContentRefresh(); // Rebuild layout with active menu highlight next frame
        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] Creating popup for menu: " << menu.id << std::endl;
        }

        // Where the main window stands, for followWindowMove to notice it move
        m_windowManager.mainWindowScreenPosition(m_lastWindowX, m_lastWindowY);

        // The anchor on the pixel its edge is drawn from (toPhysEdge), the size through
        // toPhysRound: both whole CSS px first, so they stay divisible by g_config.scale -
        // integer NS points on macOS (bsf=2). Fractional physical px produce a half-point NS
        // frame; the CAShapeLayer mask then snaps to integer points while GL renders at the
        // fractional edge, leaving a 1-px hairline at the top/right
        const fpx_t popupWidth = toPhysRound(m_windowManager.menus().cssWidthOf(menu.items));

        const fpx_t contentCssH = cssHeightOf(menu);
        const fpx_t popupHeight = toPhysRound(contentCssH);

        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] Popup height calc: contentCssH=" << contentCssH << " scaled=" << popupHeight
                      << std::endl;
        }

        const Ui::Res::Type::border_t popupRadii = m_resManager.layout().topMenuDropdown.border.scaled(g_config.scale);

        const Ui::Res::Type::bound_t popupBound = { .x = toPhysEdge(anchorXCss),
                                                    .y = toPhysEdge(anchorYCss),
                                                    .w = popupWidth,
                                                    .h = popupHeight };
        if (!m_windowManager.menus().make(popupBound, popupRadii, m_resManager.theme().second.bg)) {
            std::cerr << "[Shell] Failed to create popup window" << std::endl;
            return;
        }

        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] Popup size: " << popupWidth << "x" << popupHeight << std::endl;
        }

        // Its renderer, and the popup brought up as every popup-type window is
        // (MainFrame::present): never seen flat
        m_windowManager.menus().show(menu);

        // Mark that this popup was just activated; we want to wait one render pass
        // before honoring hover-triggered switches to avoid quick reentrancy.
        m_menuJustActivated = true;

        // Restore main window GL context after popup setup
        // (popup creation switches to popup context)
        m_windowManager.mainWindow().makeCurrent();

        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] Popup renderer initialized successfully" << std::endl;
        }
    }

    [[nodiscard]] fpx_t cssHeightOf(const Ui::Res::Type::menu_t & menu) const
    {
        return Ui::Res::Store::menuHeightOf(menu.items, m_resManager.popup());
    }

    // A press handed on to the main window at `x`, `y`, main-window physical px
    Ui::Render::element_event_t pressMainAt(int x, int y, const Ui::Window::mouse_event_t & mouse)
    {
        return m_windowManager.onMousePress(x, y, mouse.button, mouse.clickCount, mouse.modifiers);
    }

    // Total physical pixel width of all menu buttons.
    [[nodiscard]] int menusTotalWidthPhysical() const
    {
        if (!m_windowManager.hasUiRenderer()) {
            return 0;
        }

        fpx_t totalW = 0;
        for (const auto & menu : m_resManager.menus()) {
            const auto & bound = m_windowManager.elementBound(menu.id);
            totalW += bound.w;
        }

        return toPhysFloor(totalW);
    }

    // The open row menu's element when a press at these main-window px lands on its
    // row, INVALID otherwise. Asked before the popup is destroyed, which forgets it
    [[nodiscard]] id_t rowMenuUnder(int x, int y) const
    {
        if (m_rowMenuElement == Ui::INVALID_ID) {
            return Ui::INVALID_ID;
        }
        const Ui::Res::Type::bound_t & row = m_windowManager.elementBound(m_rowMenuElement);
        return row.contains(toCss(x), toCss(y)) ? m_rowMenuElement : Ui::INVALID_ID;
    }
};

} // namespace Ui
