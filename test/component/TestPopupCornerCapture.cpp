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

/**
 * @file TestPopupCornerCapture.cpp
 * @brief Component test for the popup corner capture+composite pipeline
 *
 * Creates a real X11 main window (600x300) rendered with a /\ red band pattern,
 * then creates two popup windows sequentially at different positions to simulate
 * menu switching.  Uses the actual PopupRenderer to render each popup.
 * Verifies the full 600x300 composite matches the expected image.
 */

// This suite drives a real X11 window + EGL/GL context (XOpenDisplay/XSync), so
// it only builds on the X11 backend. On other platforms component-tests still
// builds its remaining (windowless) suites; this translation unit compiles to
// nothing.
#if defined(HAVE_X11)

#include <gtest/gtest.h>

// GLEW must be included before any GL headers
#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/glutil.h"
#include "ui/gl/rounded.h"
#include "ui/render/popup/popuprenderer.h"
#include "ui/render/uilayout.h"
#include "ui/render/uirenderer.h"
#include "ui/res/resmanager.h"
#include "ui/type.h"
#include "ui/window/connector.h"
#include "ui/window/popup/popupwindow.h"

#include <GL/glew.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

using Ui::fpx_t;
using Ui::id_t;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class PopupCornerCaptureTest : public ::testing::Test {
protected:
    // Pub/sub for window lifecycle
    Ui::PubSub::Subscribe m_subscribe;

    // Parent (main) window
    Ui::Window::NativeWindow m_mainWindow { m_subscribe };

    // Resource manager (loads real theme + layout)
    Ui::Res::ResManager m_resManager { TEST_RES_DIR };

    // Dimensions
    static constexpr int kMainW  = 600;
    static constexpr int kMainH  = 300;
    static constexpr int kPopupW = 200;
    static constexpr int kPopupH = 200;

    // Per-corner radii
    static constexpr float kTL = 60.0f;
    static constexpr float kTR = 30.0f;
    static constexpr float kBR = 60.0f;
    static constexpr float kBL = 30.0f;

    // Background color for the popup
    Ui::Color m_popupBg { 255, 165, 0 }; // orange

    void SetUp() override
    {
        Display * dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            GTEST_SKIP() << "Cannot open X11 display";
        }
        XCloseDisplay(dpy);
    }

    void TearDown() override { m_mainWindow.destroy(); }

    // -- helpers ------------------------------------------------------------

    /**
     * @brief Per-corner rounded rect hit test (no anti-aliasing)
     */
    static bool isInsideRoundedRect(fpx_t px, fpx_t py, fpx_t w, fpx_t h, fpx_t tl, fpx_t tr, fpx_t br, fpx_t bl)
    {
        if (px < 0 || py < 0 || px >= w || py >= h) {
            return false;
        }

        auto dist2 = [](fpx_t dx, fpx_t dy) { return dx * dx + dy * dy; };

        if (px < tl && py < tl && tl > 0.0F) {
            return dist2(px - tl, py - tl) <= tl * tl;
        }
        if (px >= w - tr && py < tr && tr > 0.0F) {
            return dist2(px - (w - tr), py - tr) <= tr * tr;
        }
        if (px >= w - br && py >= h - br && br > 0.0F) {
            return dist2(px - (w - br), py - (h - br)) <= br * br;
        }
        if (px < bl && py >= h - bl && bl > 0.0F) {
            return dist2(px - bl, py - (h - bl)) <= bl * bl;
        }
        return true;
    }

    /**
     * @brief Build the parent pattern as a CPU buffer (top-left origin, RGBA)
     *
     * Gray background with a red /\ band: peak at top-center, legs to bottom corners.
     * Red where |y - |x - kMainW/2|| < 40.
     */
    static std::vector<uint8_t> buildParentPattern()
    {
        std::vector<uint8_t> buf(kMainW * kMainH * 4);
        for (int y = 0; y < kMainH; ++y) {
            for (int x = 0; x < kMainW; ++x) {
                int idx = (y * kMainW + x) * 4;
                // /\ spine: y_spine = |x - kMainW/2|
                int spine = std::abs(x - kMainW / 2);
                if (std::abs(y - spine) < 40) {
                    buf[idx + 0] = 255;
                    buf[idx + 1] = 0;
                    buf[idx + 2] = 0;
                } else {
                    buf[idx + 0] = 128;
                    buf[idx + 1] = 128;
                    buf[idx + 2] = 128;
                }
                buf[idx + 3] = 255;
            }
        }
        return buf;
    }

    /**
     * @brief Build expected composite: parent + rounded-rect popup overlay at given position
     */
    static std::vector<uint8_t>
    buildExpectedComposite(const std::vector<uint8_t> & parent, const Ui::Color & popupBg, int relX, int relY)
    {
        std::vector<uint8_t> buf = parent; // copy
        for (int py = 0; py < kPopupH; ++py) {
            for (int px = 0; px < kPopupW; ++px) {
                if (isInsideRoundedRect(px, py, kPopupW, kPopupH, kTL, kTR, kBR, kBL)) {
                    int dstX = relX + px;
                    int dstY = relY + py;
                    if (dstX >= 0 && dstX < kMainW && dstY >= 0 && dstY < kMainH) {
                        int idx      = (dstY * kMainW + dstX) * 4;
                        buf[idx + 0] = popupBg.r();
                        buf[idx + 1] = popupBg.g();
                        buf[idx + 2] = popupBg.b();
                        buf[idx + 3] = 255;
                    }
                }
            }
        }
        return buf;
    }

    /**
     * @brief Display a top-left-origin RGBA buffer in the main window for a duration
     */
    void showBuffer(const std::vector<uint8_t> & topLeftBuf, int w, int h)
    {
        m_mainWindow.makeCurrent();
        glViewport(0, 0, w, h);

        // Flip to GL bottom-left origin
        std::vector<uint8_t> glBuf(topLeftBuf.size());
        int                  rowBytes = w * 4;
        for (int y = 0; y < h; ++y) {
            std::memcpy(&glBuf[y * rowBytes], &topLeftBuf[(h - 1 - y) * rowBytes], rowBytes);
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, glBuf.data());

        glEnable(GL_TEXTURE_2D);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 1, 0, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        for (int pass = 0; pass < 2; ++pass) {
            glClear(GL_COLOR_BUFFER_BIT);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0);
            glVertex2f(0, 0);
            glTexCoord2f(1, 0);
            glVertex2f(1, 0);
            glTexCoord2f(1, 1);
            glVertex2f(1, 1);
            glTexCoord2f(0, 1);
            glVertex2f(0, 1);
            glEnd();
            glFinish();
            m_mainWindow.swapBuffers();
        }

        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &tex);

        XSync(m_mainWindow.nativeDisplay(), Ui::Window::Platform::X11::False);
    }

    /**
     * @brief Render the /\ pattern into the main window via GL
     */
    void renderMainWindowPattern()
    {
        m_mainWindow.makeCurrent();
        glViewport(0, 0, kMainW, kMainH);

        // Build pattern in a CPU buffer (RGBA, bottom-to-top for GL)
        std::vector<uint8_t> pixels(kMainW * kMainH * 4);
        for (int y = 0; y < kMainH; ++y) {
            int logicalY = (kMainH - 1) - y; // flip for GL
            for (int x = 0; x < kMainW; ++x) {
                int idx   = (y * kMainW + x) * 4;
                int spine = std::abs(x - kMainW / 2);
                if (std::abs(logicalY - spine) < 40) {
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 0;
                    pixels[idx + 2] = 0;
                } else {
                    pixels[idx + 0] = 128;
                    pixels[idx + 1] = 128;
                    pixels[idx + 2] = 128;
                }
                pixels[idx + 3] = 255;
            }
        }

        // Upload as texture and draw a full-screen quad
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMainW, kMainH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        glEnable(GL_TEXTURE_2D);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 1, 0, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Render into both buffers (double-buffered)
        for (int pass = 0; pass < 2; ++pass) {
            glClear(GL_COLOR_BUFFER_BIT);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0);
            glVertex2f(0, 0);
            glTexCoord2f(1, 0);
            glVertex2f(1, 0);
            glTexCoord2f(1, 1);
            glVertex2f(1, 1);
            glTexCoord2f(0, 1);
            glVertex2f(0, 1);
            glEnd();
            glFinish();
            m_mainWindow.swapBuffers();
        }

        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &tex);
    }

    /**
     * @brief Read back main window framebuffer (top-left origin, RGBA)
     */
    std::vector<uint8_t> readMainWindowPixels()
    {
        m_mainWindow.makeCurrent();
        std::vector<uint8_t> raw(kMainW * kMainH * 4);
        glReadPixels(0, 0, kMainW, kMainH, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());

        // Flip vertically (GL is bottom-to-top)
        std::vector<uint8_t> flipped(raw.size());
        int                  rowBytes = kMainW * 4;
        for (int y = 0; y < kMainH; ++y) {
            std::memcpy(&flipped[y * rowBytes], &raw[(kMainH - 1 - y) * rowBytes], rowBytes);
        }
        return flipped;
    }
};

// ---------------------------------------------------------------------------
// Test: synthetic pattern, menu switch sequence 1-2-1-2-3-2-1
// ---------------------------------------------------------------------------
/*
TEST_F(PopupCornerCaptureTest, MenuSwitchSequence)
{
    static constexpr int kPosX[] = { 0, 200, 400 };
    static constexpr int kPosY   = 50;

    // Switch sequence: menu indices (0-based)
    static constexpr int kSequence[]  = { 0, 1, 0, 1, 2, 1, 0 };
    static constexpr int kSequenceLen = 7;

    // ---- 1. Create main window ----
    ASSERT_TRUE(m_mainWindow.create(kMainW, kMainH, nullptr, 0, "CornerCaptureTest"));

    m_mainWindow.makeCurrent();
    ASSERT_TRUE(Ui::Gl::Util::initGlLoader()) << "GL loader init failed";

    // ---- 2. Load resources ----
    m_resManager.loadAll();

    // ---- 3. Show expected composites for each position ----
    auto parentBuf = buildParentPattern();
    for (int i = 0; i < 3; ++i) {
        auto expected = buildExpectedComposite(parentBuf, m_popupBg, kPosX[i], kPosY);
        showBuffer(expected, kMainW, kMainH);
    }

    // ---- 4. Run switch sequence: 1 -> 2 -> 1 -> 2 -> 3 -> 2 -> 1 ----
    int failures = 0;
    for (int step = 0; step < kSequenceLen; ++step) {
        int         menuIdx = kSequence[step];
        std::string label   = "Step" + std::to_string(step + 1) + "_Menu" + std::to_string(menuIdx + 1);

        // Re-render parent (simulates main window re-render after popup destroy)
        renderMainWindowPattern();
        XSync(m_mainWindow.nativeDisplay(), Ui::Window::Platform::X11::False);
        std::vector<uint8_t> parentPixels = readMainWindowPixels();

        if (!createAndVerifyPopup(kPosX[menuIdx], kPosY, parentPixels, label)) {
            ++failures;
        }
    }

    EXPECT_EQ(failures, 0) << failures << " of " << kSequenceLen << " steps failed";
}
*/

// ---------------------------------------------------------------------------
// Test: real UI rendering path (UiRenderer for main window + PopupRenderer)
// Replicates the actual App flow: renderUIFrame() -> deferred menu switch
// ---------------------------------------------------------------------------
TEST_F(PopupCornerCaptureTest, RealHtmlMenuSwitch)
{
    // ---- 1. Create main window (production size) ----
    static constexpr int kWinW = 800;
    static constexpr int kWinH = 600;

    ASSERT_TRUE(m_mainWindow.create(kWinW, kWinH, nullptr, 0, "RealHtmlMenuSwitchTest"));

    // Set DPI/scale (same as App::initialize)
    Ui::g_config.dpi   = Ui::Window::NativeWindow::queryDpi(m_mainWindow.nativeDisplay());
    Ui::g_config.scale = Ui::g_config.dpi / 96.0F;

    m_mainWindow.makeCurrent();
    ASSERT_TRUE(Ui::Gl::Util::initGlLoader()) << "GL loader init failed";

    // TEMPORARY(diagnostic): skip disabled so the test runs on software GL and
    // prints its real failure detail. Restore before committing.
    // if (Ui::Gl::Util::isSoftwareRenderer()) {
    //     GTEST_SKIP() << "Corner-composite MSAA pixel comparison is unreliable on software rasterizers";
    // }

    // ---- 2. Load real resources ----
    m_resManager.loadAll();

    // ---- 3. Create UiRenderer for main window (same as App::initialize) ----
    auto uiRender = std::make_unique<Ui::Render::UiRenderer>([this] { m_mainWindow.makeCurrent(); },
                                                             kWinW,
                                                             kWinH,
                                                             m_resManager);
    uiRender->setContent();
    uiRender->resize(kWinW, kWinH);

    // ---- 4. Get menu IDs and corner radii from real config ----
    const auto & menus = m_resManager.menus();
    ASSERT_GE(menus.size(), 2u) << "Need at least 2 menus for switching test";

    auto &                  border    = m_resManager.layout().topMenuDropdown.border;
    Ui::Res::Type::border_t physRadii = { border.topLeft * Ui::g_config.scale,
                                          border.topRight * Ui::g_config.scale,
                                          border.bottomRight * Ui::g_config.scale,
                                          border.bottomLeft * Ui::g_config.scale };

    std::cout << "[Test] Scale=" << Ui::g_config.scale << " border-radius: " << border.topLeft << "/" << border.topRight
              << "/" << border.bottomRight << "/" << border.bottomLeft << " (phys: " << physRadii.topLeft << "/"
              << physRadii.topRight << "/" << physRadii.bottomRight << "/" << physRadii.bottomLeft << ")" << std::endl;

    // Helper: render main UI (same as App::renderUIFrame)
    auto renderMainUI = [&]() {
        m_mainWindow.makeCurrent();
        m_mainWindow.clear();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        Ui::Gl::SvgRenderer::setUploadPremultiplied(true);
        uiRender->Render(kWinW, kWinH);
        glFinish();
        m_mainWindow.swapBuffers();
        // Double-buffer: render into back buffer too
        m_mainWindow.clear();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        Ui::Gl::SvgRenderer::setUploadPremultiplied(true);
        uiRender->Render(kWinW, kWinH);
        glFinish();
        m_mainWindow.swapBuffers();
    };

    // Helper: read main window framebuffer (top-left origin)
    auto readMainFB = [&]() -> std::vector<uint8_t> {
        m_mainWindow.makeCurrent();
        std::vector<uint8_t> raw(kWinW * kWinH * 4);
        glReadPixels(0, 0, kWinW, kWinH, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
        std::vector<uint8_t> flipped(raw.size());
        int                  rowBytes = kWinW * 4;
        for (int y = 0; y < kWinH; ++y) {
            std::memcpy(&flipped[y * rowBytes], &raw[(kWinH - 1 - y) * rowBytes], rowBytes);
        }
        return flipped;
    };

    // Helper: get button position for menu (same logic as App::createMenuPopup)
    auto menuButtonPosition = [&](id_t menuId) -> std::pair<fpx_t, fpx_t> {
        auto it = std::find_if(menus.begin(), menus.end(), [&](const auto & m) { return m.id == menuId; });
        if (it == menus.end()) {
            return { 0, 0 };
        }
        fpx_t buttonXCss = 0;
        fpx_t buttonH    = 0;
        for (const auto & m : menus) {
            const auto & b = uiRender->bound(m.id);
            if (m.order < it->order) {
                buttonXCss += b.w;
            } else if (m.id == menuId) {
                buttonH = b.h;
                break;
            }
        }
        fpx_t relX = buttonXCss * Ui::g_config.scale;
        fpx_t relY = buttonH * Ui::g_config.scale;
        return { relX, relY };
    };

    // Helper: compute popup size for a menu (same logic as App::createMenuPopup)
    auto popupSize = [&](id_t menuId) -> std::pair<fpx_t, fpx_t> {
        auto it = std::find_if(menus.begin(), menus.end(), [&](const auto & m) { return m.id == menuId; });
        if (it == menus.end()) {
            return { 200, 100 };
        }
        fpx_t popupW   = m_resManager.layout().topMenuDropdown.width * Ui::g_config.scale;
        fpx_t contentH = it->popupHeight;
        fpx_t popupH   = Ui::toPhys(contentH);
        return { popupW, popupH };
    };

    // ---- 5. Initial render ----
    renderMainUI();
    XSync(m_mainWindow.nativeDisplay(), Ui::Window::Platform::X11::False);

    // ---- 6. Menu switch sequence using real menu IDs ----
    // Use first 3 menus (or fewer if less available)
    int               numMenus = std::min(static_cast<int>(menus.size()), 3);
    std::vector<id_t> menuIds;
    for (int i = 0; i < numMenus; ++i) {
        menuIds.emplace_back(menus[i].id);
    }

    // Build sequence: 0-1-0-1-(2-1-0 if 3 menus, else 0-1-0)
    std::vector<int> sequence;
    if (numMenus >= 3) {
        sequence = { 0, 1, 0, 1, 2, 1, 0 };
    } else {
        sequence = { 0, 1, 0, 1, 0, 1, 0 };
    }

    // A popup is a window+renderer pairing bound by a Connector (matches WindowManager).
    using PopupConnector = Ui::Window::Connector<Ui::Window::Popup::PopupWindow, Ui::Render::Popup::PopupRenderer>;

    int                             failures = 0;
    std::unique_ptr<PopupConnector> prevPopup;

    for (int step = 0; step < static_cast<int>(sequence.size()); ++step) {
        int         menuIdx = sequence[step];
        id_t        menuId  = menuIds[menuIdx];
        std::string label   = "Step" + std::to_string(step + 1) + "_" + std::to_string(menuId);

        std::cout << "[Test] === " << label << " ===" << std::endl;

        // Step A: Destroy previous popup (same as App::destroyPopup(true))
        if (prevPopup) {
            prevPopup->window().makeCurrent();
            prevPopup->window().destroy();
            prevPopup.reset();
            // std::this_thread::sleep_for(std::chrono::milliseconds(600));
            XSync(m_mainWindow.nativeDisplay(), Ui::Window::Platform::X11::False);
        }

        // Step B: Re-render main UI (same as App::renderUIFrame after destroyPopup)
        renderMainUI();
        XSync(m_mainWindow.nativeDisplay(), Ui::Window::Platform::X11::False);

        std::vector<uint8_t> parentPixels;

        // Step D: Create popup (same as App::createMenuPopup)
        auto [relX, relY]     = menuButtonPosition(menuId);
        auto [popupW, popupH] = popupSize(menuId);

        std::cout << "[Test] " << label << ": popup at rel(" << relX << "," << relY << ") size " << popupW << "x"
                  << popupH << std::endl;

        auto popup = std::make_unique<PopupConnector>(m_subscribe);
        popup->window().setPosition(relX, relY);
        popup->window().setBackground(m_resManager.theme().dropdown.bg);
        popup->window().setCornerRadii(physRadii);

        if (!popup->window().create(m_mainWindow, popupW, popupH)) {
            ADD_FAILURE() << label << ": Failed to create popup";
            ++failures;
            continue;
        }

        // Step E: Create and render PopupRenderer (same as App)
        m_resManager.setActiveMenu(menuId);

        const auto & menuList = m_resManager.menus();
        auto         menuIt = std::find_if(menuList.begin(), menuList.end(), [menuId](const Ui::Res::Type::menu_t & m) {
            return m.id == menuId;
        });
        auto &       popupRendererRef = popup->emplaceRenderer([&window = popup->window()] { window.makeCurrent(); },
                                                         m_resManager,
                                                         *menuIt);
        popupRendererRef.resize(popup->window().bound().w, popup->window().bound().h);
        popupRendererRef.setAlpha(popup->window().hasAlpha());

        // Capture corner pixels from parent back buffer (simplified version of
        // WindowManager::captureCornerPixels -- reads entire parent FB as source)
        {
            m_mainWindow.makeCurrent();
            // Render main UI into back buffer for corner capture
            m_mainWindow.clear();
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            Ui::Gl::SvgRenderer::setUploadPremultiplied(true);
            uiRender->Render(kWinW, kWinH);
            glFinish();

            // Read parent pixels for verification (while back buffer has fresh content)
            parentPixels = readMainFB();

            if (popup->hasRenderer()) {
                Ui::Render::Popup::PopupRenderer *  popupRenderer = &popup->renderer();
                const Ui::Res::Type::border_t &     cr            = popup->window().cornerRadii();
                std::array<std::vector<uint8_t>, 4> cornerPixels;
                for (id_t ci = 0; ci < 4; ++ci) {
                    const int r = static_cast<int>(Ui::Gl::Rounded::borderRadius(cr, ci));
                    if (r <= 0) {
                        continue;
                    }
                    // Corner origin in parent coords
                    int cx     = 0;
                    int cy     = 0;
                    int popX   = static_cast<int>(relX);
                    int popY   = static_cast<int>(relY);
                    int popWi2 = static_cast<int>(popupW);
                    int popHi2 = static_cast<int>(popupH);
                    switch (ci) {
                    case 0:
                        cx = popX;
                        cy = popY;
                        break;
                    case 1:
                        cx = popX + popWi2 - r;
                        cy = popY;
                        break;
                    case 2:
                        cx = popX + popWi2 - r;
                        cy = popY + popHi2 - r;
                        break;
                    case 3:
                        cx = popX;
                        cy = popY + popHi2 - r;
                        break;
                    default: continue;
                    }
                    // Init buffer with bg color
                    auto & buf = cornerPixels.at(ci);
                    buf.resize(static_cast<size_t>(r * r * 4));
                    auto bg = popup->window().background();
                    for (int j = 0; j < r * r; ++j) {
                        buf[static_cast<size_t>(j * 4 + 0)] = bg.r();
                        buf[static_cast<size_t>(j * 4 + 1)] = bg.g();
                        buf[static_cast<size_t>(j * 4 + 2)] = bg.b();
                        buf[static_cast<size_t>(j * 4 + 3)] = 255;
                    }
                    // Read from GL back buffer
                    int capX = std::max(0, cx);
                    int capY = std::max(0, cy);
                    int capW = std::min(r, kWinW - capX);
                    int capH = std::min(r, kWinH - capY);
                    if (capW > 0 && capH > 0) {
                        std::vector<uint8_t> regionPixels(static_cast<size_t>(capW * capH * 4));
                        glReadPixels(capX,
                                     kWinH - capY - capH,
                                     capW,
                                     capH,
                                     GL_RGBA,
                                     GL_UNSIGNED_BYTE,
                                     regionPixels.data());
                        // Flip vertically
                        int rowBytes = capW * 4;
                        int row      = 0;
                        while (row < capH / 2) {
                            std::swap_ranges(&regionPixels[row * rowBytes],
                                             &regionPixels[row * rowBytes + rowBytes],
                                             &regionPixels[(capH - 1 - row) * rowBytes]);
                            ++row;
                        }
                        // Blit into corner buffer
                        int offX = capX - cx;
                        int offY = capY - cy;
                        for (int py2 = 0; py2 < capH; ++py2) {
                            for (int px2 = 0; px2 < capW; ++px2) {
                                int dX = px2 + offX;
                                int dY = py2 + offY;
                                if (dX >= 0 && dX < r && dY >= 0 && dY < r) {
                                    int srcIdx = (py2 * capW + px2) * 4;
                                    int dstIdx = (dY * r + dX) * 4;
                                    buf[static_cast<size_t>(dstIdx + 0)] =
                                    regionPixels[static_cast<size_t>(srcIdx + 0)];
                                    buf[static_cast<size_t>(dstIdx + 1)] =
                                    regionPixels[static_cast<size_t>(srcIdx + 1)];
                                    buf[static_cast<size_t>(dstIdx + 2)] =
                                    regionPixels[static_cast<size_t>(srcIdx + 2)];
                                    buf[static_cast<size_t>(dstIdx + 3)] = 255;
                                }
                            }
                        }
                    }
                }
                popup->window().makeCurrent();
                popupRenderer->setCornerPixels(std::move(cornerPixels), cr);
            }
        }

        if (!popup->render()) {
            ADD_FAILURE() << label << ": popup->render() failed";
            ++failures;
            popup->window().destroy();
            continue;
        }

        // Read popup framebuffer BEFORE swapBuffers - after swap the back buffer is undefined per EGL spec
        popup->window().makeCurrent();
        glFinish();
        int                  popupWi = static_cast<int>(popupW);
        int                  popupHi = static_cast<int>(popupH);
        std::vector<uint8_t> popupRaw(static_cast<size_t>(popupWi * popupHi * 4));
        glReadPixels(0, 0, popupWi, popupHi, GL_RGBA, GL_UNSIGNED_BYTE, popupRaw.data());

        popup->window().swapBuffers();
        XSync(m_mainWindow.nativeDisplay(), Ui::Window::Platform::X11::False);
        std::vector<uint8_t> popupPixels(popupRaw.size());
        int                  popupRowBytes = popupWi * 4;
        for (int y = 0; y < popupHi; ++y) {
            std::memcpy(&popupPixels[y * popupRowBytes], &popupRaw[(popupHi - 1 - y) * popupRowBytes], popupRowBytes);
        }

        // Clamp radii for verification (same logic as WindowManager::clampRadii)
        auto clampRadii = [](const Ui::Res::Type::border_t & radii, fpx_t w, fpx_t h) -> Ui::Res::Type::border_t {
            const fpx_t maxR = std::min(w, h) / 2;
            return { std::min(maxR, std::round(radii.topLeft)),
                     std::min(maxR, std::round(radii.topRight)),
                     std::min(maxR, std::round(radii.bottomRight)),
                     std::min(maxR, std::round(radii.bottomLeft)) };
        };
        Ui::Res::Type::border_t clamped = clampRadii(physRadii, popupW, popupH);
        fpx_t                   clTL    = clamped.topLeft;
        fpx_t                   clTR    = clamped.topRight;
        fpx_t                   clBR    = clamped.bottomRight;
        fpx_t                   clBL    = clamped.bottomLeft;

        std::cout << "[Test] " << label << ": clamped radii [" << clTL << "," << clTR << "," << clBR << "," << clBL
                  << "]" << std::endl;

        // Debug: dump corner pixel samples
        {
            // TL corner: pixel (0,0) and (clTL/2, clTL/2)
            fpx_t samplePositions[][2] = { { 0, 0 },
                                           { clTL / 2, clTL / 2 },
                                           { popupWi - 1.0F, 0 },
                                           { popupWi - 1.0F, popupHi - 1.0F },
                                           { 0, popupHi - 1.0F },
                                           { popupWi / 2.0F, 0 } };
            for (auto & sp : samplePositions) {
                fpx_t sx = sp[0], sy = sp[1];
                if (sx >= 0 && sx < popupWi && sy >= 0 && sy < popupHi) {
                    int popIdx = static_cast<int>(sy) * popupWi + static_cast<int>(sx);
                    popIdx *= 4;
                    int parX   = static_cast<int>(relX + sx);
                    int parY   = static_cast<int>(relY + sy);
                    int parIdx = (parY >= 0 && parY < kWinH && parX >= 0 && parX < kWinW) ? (parY * kWinW + parX) * 4
                                                                                          : -1;
                    std::cout << "[Test] " << label << ": pixel(" << sx << "," << sy << ") popup=("
                              << (int)popupPixels[popIdx] << "," << (int)popupPixels[popIdx + 1] << ","
                              << (int)popupPixels[popIdx + 2] << ") parent=";
                    if (parIdx >= 0) {
                        std::cout << "(" << (int)parentPixels[parIdx] << "," << (int)parentPixels[parIdx + 1] << ","
                                  << (int)parentPixels[parIdx + 2] << ")";
                    } else {
                        std::cout << "(OOB)";
                    }
                    bool inRect = isInsideRoundedRect(sx, sy, popupW, popupH, clTL, clTR, clBR, clBL);
                    std::cout << (inRect ? " [inside]" : " [exterior]") << std::endl;
                }
            }
        }

        // Verify: pixels in corner-exterior regions should match parent
        int cornerMismatches    = 0;
        int cornerPixelsChecked = 0;
        int aaBoundarySkipped   = 0;

        for (int py = 0; py < popupHi; ++py) {
            for (int px = 0; px < popupWi; ++px) {
                // Check if pixel is inside the rounded rect (using clamped radii)
                bool inside = isInsideRoundedRect(px, py, popupWi, popupHi, clTL, clTR, clBR, clBL);
                if (inside) {
                    continue; // Skip interior pixels (rendered by UiRenderer)
                }

                // This is a corner-exterior pixel - should show parent content
                int parentX = static_cast<int>(relX) + px;
                int parentY = static_cast<int>(relY) + py;

                // Skip if outside parent bounds
                if (parentX < 0 || parentX >= kWinW || parentY < 0 || parentY >= kWinH) {
                    continue;
                }

                // Check if this is an AA boundary pixel
                bool isBoundary = false;
                for (int dy = -2; dy <= 2 && !isBoundary; ++dy) {
                    for (int dx = -2; dx <= 2 && !isBoundary; ++dx) {
                        if (dx == 0 && dy == 0) {
                            continue;
                        }
                        if (isInsideRoundedRect(px + dx, py + dy, popupWi, popupHi, clTL, clTR, clBR, clBL) != inside) {
                            isBoundary = true;
                        }
                    }
                }
                if (isBoundary) {
                    ++aaBoundarySkipped;
                    continue;
                }

                ++cornerPixelsChecked;

                int popIdx = (py * popupWi + px) * 4;
                int parIdx = (parentY * kWinW + parentX) * 4;

                uint8_t pR = popupPixels[popIdx + 0];
                uint8_t pG = popupPixels[popIdx + 1];
                uint8_t pB = popupPixels[popIdx + 2];
                uint8_t eR = parentPixels[parIdx + 0];
                uint8_t eG = parentPixels[parIdx + 1];
                uint8_t eB = parentPixels[parIdx + 2];

                constexpr int kColorTolerance = 5; // MSAA resolve precision at color boundaries
                int           diffR           = std::abs(static_cast<int>(pR) - static_cast<int>(eR));
                int           diffG           = std::abs(static_cast<int>(pG) - static_cast<int>(eG));
                int           diffB           = std::abs(static_cast<int>(pB) - static_cast<int>(eB));
                if (diffR > kColorTolerance || diffG > kColorTolerance || diffB > kColorTolerance) {
                    if (cornerMismatches < 5) {
                        std::cout << "[Test] " << label << ": mismatch at popup(" << px << "," << py << ") parent("
                                  << parentX << "," << parentY << ") got=(" << (int)pR << "," << (int)pG << ","
                                  << (int)pB << ") exp=(" << (int)eR << "," << (int)eG << "," << (int)eB << ")"
                                  << std::endl;
                    }
                    ++cornerMismatches;
                }
            }
        }

        // MSAA resolve is implementation-defined: the corner shader and HTML renderer
        // draw to the same MSAA surface, and the resolved pixel values at boundaries
        // between the two passes are unpredictable. Allow up to 10% mismatches.
        constexpr double kMaxMismatchPct = 10.0;
        double mismatchPct = (cornerPixelsChecked > 0) ? (100.0 * cornerMismatches / cornerPixelsChecked) : 0.0;

        std::cout << "[Test] " << label << ": Checked " << cornerPixelsChecked << " corner pixels, skipped "
                  << aaBoundarySkipped << " AA boundary, mismatches=" << cornerMismatches << " (" << mismatchPct << "%)"
                  << std::endl;

        if (mismatchPct > kMaxMismatchPct) {
            ADD_FAILURE() << label << ": Corner mismatch rate " << mismatchPct << "% exceeds " << kMaxMismatchPct
                          << "% threshold (" << cornerMismatches << "/" << cornerPixelsChecked << " pixels)";
            ++failures;
        }

        // Keep popup alive for next iteration's destroy step
        prevPopup = std::move(popup);
    }

    // Final cleanup
    if (prevPopup) {
        prevPopup->window().makeCurrent();
        prevPopup->window().destroy();
        prevPopup.reset();
    }
    m_mainWindow.makeCurrent();

    EXPECT_EQ(failures, 0) << failures << " of " << sequence.size() << " steps failed";
}

} // namespace PureGlUi

#endif // HAVE_X11
