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
#include "ui/color.h"
#include "ui/config.h"
#include "ui/render/popup/popuprenderer.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/res/store/menuheight.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/menu.h"
#include "ui/type.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/menuconnector.h"
#include "ui/window/connector/pairing.h"
#include "ui/window/mainframe.h"
#include "ui/window/menucornersof.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/popupslot.h"
#include "ui/window/popupwindows.h"
#include "ui/window/underlay.h"
#include "ui/window/windowmanagerdebug.h"

#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#ifdef HAVE_X11
#include "ui/window/platform/x11include.h"
#endif

namespace Ui::Window {

// The menu - a top-bar dropdown or a dock row's - and the submenu a hovered item opens
// beside it: their windows made, laid out, brought up, moved with the main window and
// closed. The windows are the WindowManager's, two slots it hands in; what opens which
// menu, and when, is the shell's (Ui::MenuBar)
class MenuPopups final : private Common::NonCopyable {
public:
    MenuPopups(const Ui::Res::ResManager &            resManager,
               const std::unique_ptr<MainConnector> & main,
               PopupWindows &                         popups,
               Underlay &                             underlay,
               MainFrame &                            mainFrame,
               popup_slot_t<MenuConnector> &          menu,
               popup_slot_t<MenuConnector> &          submenu)
        : m_resManager(resManager)
        , m_main(main)
        , m_popups(popups)
        , m_underlay(underlay)
        , m_mainFrame(mainFrame)
        , m_menu(menu)
        , m_submenu(submenu)
    {
    }

    // The menu's window is up
    [[nodiscard]] bool isOpen() const { return m_menu.connector && m_menu.connector->window().isValid(); }

    [[nodiscard]] bool hasSubmenu() const { return m_submenu.connector && m_submenu.connector->window().isValid(); }

    // The menu's window, nullptr with none
    [[nodiscard]] Ui::Window::Popup::PopupWindow * window() const
    {
        return m_menu.connector ? &m_menu.connector->window() : nullptr;
    }

    // The submenu's window, nullptr with none
    [[nodiscard]] Ui::Window::Popup::PopupWindow * submenuWindow() const
    {
        return m_submenu.connector ? &m_submenu.connector->window() : nullptr;
    }

    // True when the open popup is an X11 child of the main window and so tracks
    // the parent's move/resize itself - the shell must not move/recreate it.
    [[nodiscard]] bool followsParent() const { return m_menu.connector && m_menu.connector->window().followsParent(); }

    // The menu, where it is no child the platform carries with the main window, hidden while
    // that window resizes - made again once the resize settles
    void hideForResize() const
    {
        if (m_menu.connector && !followsParent()) {
            m_menu.connector->window().hide();
        }
    }

    // The menu and its submenu moved with the main window, `deltaX`, `deltaY` physical px: a
    // rigid move keeps the background captured behind their corners valid, so nothing is
    // recaptured
    void moveBy(int deltaX, int deltaY)
    {
        const auto moveSlot = [deltaX, deltaY](popup_slot_t<MenuConnector> & slot) {
            if (slot.connector) {
                auto & window = slot.connector->window();
                window.move(window.bound().x + deltaX, window.bound().y + deltaY);
            }
        };
        moveSlot(m_menu);
        moveSlot(m_submenu);
    }

    // Whether `x`, `y`, menu-local physical px, is within the menu's rect
    [[nodiscard]] bool contains(int x, int y) const
    {
        if (!m_menu.connector) {
            return false;
        }
        const Ui::Res::Type::bound_t & bound = m_menu.connector->window().bound();
        return x >= 0 && x < bound.w && y >= 0 && y < bound.h;
    }

    // Menu-local physical px as main-window ones: as they are with no menu
    [[nodiscard]] std::pair<int, int> mainPointOf(int x, int y) const
    {
        if (!m_menu.connector) {
            return { x, y };
        }
        const Ui::Res::Type::bound_t & bound = m_menu.connector->window().bound();
        return { static_cast<int>(bound.x) + x, static_cast<int>(bound.y) + y };
    }

    // A popup's CSS width, for a top-level popup and a submenu alike. The main
    // context has to be current: a glyph measured for the first time is uploaded
    // into the main renderer's atlas
    [[nodiscard]] fpx_t cssWidthOf(const std::vector<Ui::Res::Type::menu_t> & items)
    {
        m_main->window().makeCurrent();
        return m_main->renderer().popupWidthOf(items);
    }

    // The menu's window at bound - physical px from the main window's content corner, as the
    // popup keeps it, each platform placing it on screen from there - cut to radii
    bool make(const Ui::Res::Type::bound_t & bound, const Ui::Res::Type::border_t & radii, const Ui::Color & bgColor)
    {
        if (m_menu.connector) {
            // Sync display to ensure old popup is removed before capturing corners for new one
            close(true);
        }

        if (!m_popups.make(bound, radii, bgColor, false, [this]() { m_underlay.recapture(m_menu); }, m_menu)) {
            std::cerr << "[WindowManager] Failed to create popup window at " << bound.x << "," << bound.y << std::endl;
            return false;
        }

        if constexpr (WM_ROUTE_DEBUG) {
            std::cout << "[WindowManager] Created popup at " << bound.x << "," << bound.y << " size " << bound.w << "x"
                      << bound.h << std::endl;
        }
        return true;
    }

    // The menu make() made, given its renderer and brought up (MainFrame::present). The menu
    // is handed in rather than looked up among the top-bar menus: a dock row's menu is a
    // node anywhere in the tree, and both kinds render the same way
    void show(const Ui::Res::Type::menu_t & menu)
    {
        if (!m_menu.connector) {
            return;
        }
        Ui::Render::Popup::PopupRenderer & menuRenderer = emplaceRenderer(m_menu, menu);
        menuRenderer.setSubmenuHoverCallback(
            [this](const Ui::Res::Type::bound_t & itemBound,
                   const Ui::Res::Type::menu_t &  item,
                   bool                           isFirst,
                   bool                           isLast) { followItemHover(itemBound, item, isFirst, isLast); });
        m_mainFrame.present(m_menu);
    }

    /**
     * @brief Close the menu and its submenu
     * @param syncDisplay If true, sync with X server to ensure window is removed before returning
     */
    void close(bool syncDisplay = false)
    {
        closeSubmenu();

        if (m_menu.connector) {
            // reset() triggers destructor which calls cleanupWayland() (xdg_popup,
            // xdg_surface) then Platform::WaylandWindow::destroy() (wl_surface, EGL).
            // Both must happen before the sync to avoid "invalid object" errors.
            m_popups.close(m_menu);

            // Sync to ensure the popup is removed from screen before we capture corners
            // for a new popup (otherwise we capture the old popup content)
            if (syncDisplay && m_main) {
#ifdef HAVE_X11
                auto * display = m_main->window().nativeDisplay(); // NOLINT(cppcoreguidelines-init-variables)
                if (display != nullptr) {
                    XSync(display, Ui::Window::Platform::X11::False);
                }
#elifdef HAVE_WAYLAND
                if (m_main->window().nativeDisplay() != nullptr) {
                    wl_display_roundtrip(m_main->window().nativeDisplay());
                }
#endif
            }

            if constexpr (WM_ROUTE_DEBUG) {
                std::cout << "[WindowManager] Destroyed popup window" << std::endl;
            }
        }
    }

    // Re-open a submenu for the given parent item id, if a popup is currently
    // shown and contains that item. Used to restore submenu state after a
    // resource reload, where the popup was destroyed and recreated.
    void reopenSubmenu(id_t parentItemId)
    {
        if (parentItemId == Ui::INVALID_ID || !m_menu.connector) {
            return;
        }
        auto * menuRenderer = renderer();
        if (menuRenderer == nullptr) {
            return;
        }
        const Ui::Render::UiElement * el = menuRenderer->findElementById(parentItemId);
        if (el == nullptr) {
            return;
        }
        const Ui::Res::Type::menu_t item = m_resManager.findMenuItem(parentItemId);
        if (item.items.empty()) {
            return;
        }
        const Ui::Res::Type::bound_t physBound = toPhysRound(el->bound);
        const bool                   isFirst   = menuRenderer->isFirstElement(parentItemId);
        const bool                   isLast    = menuRenderer->isLastElement(parentItemId);
        followItemHover(physBound, item, isFirst, isLast);
    }

    // Highlighted item across the open popup + submenu (submenu wins, being the
    // deeper window). Saved/restored across a reload that recreates the windows.
    [[nodiscard]] id_t activeItemId() const
    {
        if (auto * menuRenderer = submenuRenderer(); menuRenderer != nullptr) {
            if (const id_t id = menuRenderer->hoveredItemId(); id != Ui::INVALID_ID) {
                return id;
            }
        }
        if (auto * menuRenderer = renderer(); menuRenderer != nullptr) {
            return menuRenderer->hoveredItemId();
        }
        return Ui::INVALID_ID;
    }

    // Re-highlight an item on whichever of the popup/submenu actually holds it.
    void setActiveItem(id_t id)
    {
        if (auto * menuRenderer = submenuRenderer();
            menuRenderer != nullptr && menuRenderer->findElementById(id) != nullptr) {
            menuRenderer->setHoveredItem(id);
            m_submenu.connector->window().requestRender();
            return;
        }
        if (auto * menuRenderer = renderer(); menuRenderer != nullptr && menuRenderer->findElementById(id) != nullptr) {
            menuRenderer->setHoveredItem(id);
            m_menu.connector->window().requestRender();
        }
    }

    // The pointer on the main window, off both: their hover cleared
    void leave() const
    {
        if (m_menu.connector) {
            m_menu.connector->pairing().onMouseLeave();
            m_menu.connector->window().requestRender();
        }
        if (m_submenu.connector) {
            m_submenu.connector->pairing().onMouseLeave();
            m_submenu.connector->window().requestRender();
        }
    }

private:
    const Ui::Res::ResManager &            m_resManager;
    const std::unique_ptr<MainConnector> & m_main;
    PopupWindows &                         m_popups;
    Underlay &                             m_underlay;
    MainFrame &                            m_mainFrame;
    popup_slot_t<MenuConnector> &          m_menu;
    popup_slot_t<MenuConnector> &          m_submenu;
    id_t                                   m_submenuItemId = Ui::INVALID_ID; // the menu's item the submenu opens from

    // The menu/submenu connectors both own a PopupRenderer; resolve it (nullptr
    // if the window or its renderer is not yet created).
    [[nodiscard]] Ui::Render::Popup::PopupRenderer * renderer() const
    {
        return (m_menu.connector && m_menu.connector->hasRenderer()) ? &m_menu.connector->renderer() : nullptr;
    }

    [[nodiscard]] Ui::Render::Popup::PopupRenderer * submenuRenderer() const
    {
        return (m_submenu.connector && m_submenu.connector->hasRenderer()) ? &m_submenu.connector->renderer() : nullptr;
    }

    // A menu's or a submenu's renderer in `slot`, `menu` laid out at the window's width
    Ui::Render::Popup::PopupRenderer & emplaceRenderer(popup_slot_t<MenuConnector> & slot,
                                                       const Ui::Res::Type::menu_t & menu)
    {
        return m_popups.emplaceRenderer(slot, menu, toCss(slot.connector->window().bound().w));
    }

    // The menu's right-hand corners square where an open submenu meets it - beside its first
    // item the top one, its last the bottom - or round again (menuCornersOf). A square corner
    // draws no pixels captured under it
    void setCorners(bool isTopCut, bool isBottomCut)
    {
        const Ui::Res::Type::border_t & border = m_resManager.layout().topMenuDropdown.border;
        m_menu.connector->window().setCornerRadii(menuCornersOf(border.scaled(g_config.scale), isTopCut, isBottomCut));
        auto * menuRenderer = renderer();
        if (menuRenderer == nullptr) {
            return;
        }
        menuRenderer->setContainerBorder(menuCornersOf(border, isTopCut, isBottomCut));
        m_menu.connector->window().makeCurrent();
        if (isTopCut) {
            menuRenderer->updateCorner(1, {}, 0);
        }
        if (isBottomCut) {
            menuRenderer->updateCorner(2, {}, 0);
        }
        m_main->window().makeCurrent();
    }

    /**
     * @brief Destroy submenu popup window
     */
    void closeSubmenu()
    {
        // Clear submenu parent highlight from parent popup renderer
        highlightSubmenuParent(Ui::INVALID_ID);

        if (m_submenu.connector) {
            m_popups.close(m_submenu);
            // m_submenuItemId is the *caller's* state. Resetting it here would
            // overwrite the value the caller set when switching to a new
            // submenu (makeSubmenu calls us mid-transition with the new id
            // already in m_submenuItemId).

            // The menu's corners round again, recaptured off a fresh main frame
            if (m_menu.connector && m_main) {
                setCorners(false, false);
                m_mainFrame.recapture(m_menu);
            }

            if constexpr (WM_ROUTE_DEBUG) {
                std::cout << "[WindowManager] Destroyed submenu window" << std::endl;
            }
        }
    }

    void highlightSubmenuParent(id_t parentId)
    {
        if (!m_menu.connector) {
            return;
        }
        if (auto * menuRenderer = renderer(); menuRenderer != nullptr) {
            menuRenderer->setSubmenuParentId(parentId);
            m_menu.connector->window().requestRender();
        }
    }

    /**
     * @brief Create submenu popup at screen coordinates
     */
    bool makeSubmenu(const Ui::Res::Type::bound_t &  bound,
                     const Ui::Res::Type::border_t & radii,
                     const Ui::Color &               bgColor)
    {
        closeSubmenu();

        int parentScreenX {};
        int parentScreenY {};
        m_main->window().screenPosition(parentScreenX, parentScreenY);

        if (!m_popups.make(
                { .x = bound.x - parentScreenX, .y = bound.y - parentScreenY, .w = bound.w, .h = bound.h },
                radii,
                bgColor,
                false,
                [this]() { m_underlay.recapture(m_submenu); },
                m_submenu)) {
            std::cerr << "[WindowManager] Failed to create submenu window" << std::endl;
            return false;
        }

        if constexpr (WM_ROUTE_DEBUG) {
            std::cout << "[WindowManager] Created submenu at screen(" << bound.x << "," << bound.y << ") size "
                      << bound.w << "x" << bound.h << std::endl;
        }
        return true;
    }

    // The submenu makeSubmenu() made, given its renderer and brought up (MainFrame::present)
    void showSubmenu(const Ui::Res::Type::menu_t & submenu)
    {
        if (!m_submenu.connector) {
            return;
        }
        emplaceRenderer(m_submenu, submenu);
        m_mainFrame.present(m_submenu);
    }

    /**
     * @brief Handle submenu hover from PopupRenderer callback
     *
     * Opens a submenu to the right of the hovered item, or closes it
     * when hovering a non-submenu item (empty submenus).
     */
    void followItemHover(const Ui::Res::Type::bound_t & itemBound,
                         const Ui::Res::Type::menu_t &  item,
                         bool                           isFirst,
                         bool                           isLast)
    {
        // Close submenu when hovering a non-submenu item
        if (item.items.empty()) {
            if (m_submenuItemId != Ui::INVALID_ID) {
                m_submenuItemId = Ui::INVALID_ID;
                closeSubmenu();
            }
            return;
        }

        // Skip if submenu is already open for this item
        if (item.id == m_submenuItemId && m_submenu.connector) {
            return;
        }
        m_submenuItemId = item.id;

        // Calculate submenu position: to the right of the hovered item
        const Ui::Res::Type::bound_t popupBound = m_menu.connector->window().bound();

        int parentScreenX {};
        int parentScreenY {};
        m_main->window().screenPosition(parentScreenX, parentScreenY);

        const fpx_t submenuX = parentScreenX + popupBound.x + popupBound.w;
        const fpx_t submenuY = parentScreenY + popupBound.y + itemBound.y;

        // Calculate submenu size from items
        const fpx_t submenuCssW = cssWidthOf(item.items);
        const fpx_t contentCssH = Ui::Res::Store::menuHeightOf(item.items, m_resManager.popup());
        // Round to integer physical px divisible by g_config.scale (see Ui::MenuBar::show)
        const fpx_t submenuW = toPhysRound(submenuCssW);
        const fpx_t submenuH = toPhysRound(contentCssH);

        Ui::Res::Type::border_t submenuRadii = m_resManager.layout().topMenuDropdown.border.scaled(g_config.scale);

        // Submenu TL is always flat (connects to parent right edge)
        submenuRadii.topLeft = 0;

        if (isFirst || isLast) {
            setCorners(isFirst, isLast);
            m_menu.connector->window().requestRender();
        }

        const Ui::Res::Type::bound_t submenuBound = { .x = submenuX, .y = submenuY, .w = submenuW, .h = submenuH };
        if (!makeSubmenu(submenuBound, submenuRadii, m_resManager.theme().second.bg)) {
            return;
        }

        Ui::Res::Type::menu_t submenu;
        submenu.items       = item.items;
        submenu.popupHeight = contentCssH;
        showSubmenu(submenu);
        if (!m_submenu.connector) {
            return;
        }

        // Mark submenu parent in parent popup so it stays highlighted
        highlightSubmenuParent(item.id);

        m_main->window().makeCurrent();
    }
};

} // namespace Ui::Window
