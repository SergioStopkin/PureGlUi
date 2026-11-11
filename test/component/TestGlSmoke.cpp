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
 * @file TestGlSmoke.cpp
 * @brief Platform-neutral GL smoke test.
 *
 * Exercises the real window -> GL context -> (connector -> UiRenderer) -> readback
 * path using only the framework's abstract seam (NativeWindow / Connector /
 * UiRenderer) - no Xlib, no platform-specific corner machinery. So it runs on
 * X11, macOS, and any platform that can hand out a modern GL context.
 *
 * Design notes:
 * - One window / one context / one loader-init for the whole test (matching the
 *   app and TestPopupCornerCapture); recreating an EGL context in-process before
 *   the first glewInit trips a fatal X error, so we do not churn contexts.
 * - The known-color readback uses an offscreen FBO, not the window's back buffer:
 *   macOS binds the NSOpenGL drawable only on show(), so an unshown window's back
 *   buffer reads black. An FBO is drawable-independent and deterministic on every
 *   platform, so the pixel assertion holds identically everywhere.
 *
 * Graceful degradation:
 * - no window/context (truly headless, no GL driver) -> SKIP
 * - legacy-only context (e.g. Microsoft's software GL 1.1 on a GPU-less Windows
 *   runner: no FBOs, no shaders) -> context liveness verified, render+readback skipped.
 */

#include "isdisplayset.h"
#include "testconst.h"
#include "ui/color.h"
#include "ui/gl/glrender.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/pubsub/subscribe.h"
#include "ui/render/popup/tooltiprenderer.h"
#include "ui/render/uirenderer.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/alignh.h"
#include "ui/res/type/alignv.h"
#include "ui/type.h"
#include "ui/window/connector.h"
#include "ui/window/nativewindow.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

using Ui::Window::NativeWindow;
using MainConnector = Ui::Window::Connector<NativeWindow, Ui::Render::UiRenderer>;

// Drives the real framework path on one window/context:
//   1. context is live
//   2. clear + readback of a known color via an offscreen FBO (deterministic)
//   3. Connector<NativeWindow, UiRenderer> renders one UI frame
TEST(GlSmoke, WindowContextAndUiRender)
{
    Ui::PubSub::Subscribe subscribe;
    auto                  connector = std::make_unique<MainConnector>(subscribe);

    if (!connector->window().create(320, 240, nullptr, 0, "gl-smoke")) {
        ASSERT_FALSE(isDisplaySet()) << DISPLAY_UNREACHABLE;
        GTEST_SKIP() << "No window/GL context available (headless without a GL driver)";
    }
    NativeWindow & window = connector->window();
    window.makeCurrent();

    ASSERT_TRUE(Ui::Gl::Util::initGlLoader()) << "GL loader init failed";
    ASSERT_NE(glGetString(GL_VERSION), nullptr) << "no live GL context";

    if (!Ui::Gl::Util::hasModernGl()) {
        GTEST_SKIP() << "GL context live; modern GL (>= 3.x) unavailable - render + readback not exercised";
    }

    // (1) Known-color clear + readback via an offscreen FBO (drawable-independent,
    //     so it reads back identically on X11, macOS, and Windows).
    GLuint fbo    = 0;
    GLuint fboTex = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &fboTex);
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE));

    glViewport(0, 0, 64, 64);
    glClearColor(0.2F, 0.4F, 0.6F, 1.0F); // -> 51, 102, 153
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();

    std::array<uint8_t, 4> pixel {};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_NEAR(pixel[0], 51, 3);
    EXPECT_NEAR(pixel[1], 102, 3);
    EXPECT_NEAR(pixel[2], 153, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &fboTex);
    glDeleteFramebuffers(1, &fbo);

    // (2) Full framework render: layout -> GlRender -> shaders -> fonts. Renders
    // to the window's default framebuffer; we assert it produces a frame (we do
    // not read the window back buffer - see the drawable note in the file header).
    Ui::Res::ResManager resManager { TEST_RES_DIR };

    // emplaceRenderer makes the window's context current, then constructs the
    // UiRenderer (makeCurrent callback + physical size + resources).
    auto & renderer = connector->emplaceRenderer(
    std::make_unique<Ui::Gl::GlRender>([&window] { window.makeCurrent(); }, resManager.resPath().fontDir()),
    320.0F,
    240.0F,
    resManager);
    renderer.setContent();

    EXPECT_TRUE(connector->render()); // connector makes current, then UiRenderer draws

    // Connector dtor tears the renderer down with the context current, then the
    // window - no manual destroy needed.
}

// A window's context, and a line of text drawn in it into an offscreen buffer over an opaque ground
class TextFrameTest : public ::testing::Test {
protected:
    Ui::PubSub::Subscribe          m_subscribe;
    std::unique_ptr<MainConnector> m_connector = std::make_unique<MainConnector>(m_subscribe);
    Ui::Res::ResManager            m_resManager { TEST_RES_DIR };

    void SetUp() override
    {
        if (!m_connector->window().create(TEXT_FRAME_W, TEXT_FRAME_H, nullptr, 0, "gl-text")) {
            ASSERT_FALSE(isDisplaySet()) << DISPLAY_UNREACHABLE;
            GTEST_SKIP() << "No window/GL context available (headless without a GL driver)";
        }
        m_connector->window().makeCurrent();
        ASSERT_TRUE(Ui::Gl::Util::initGlLoader()) << "GL loader init failed";
        if (!Ui::Gl::Util::hasModernGl()) {
            GTEST_SKIP() << "modern GL (>= 3.x) unavailable - nothing is drawn to read";
        }
        m_resManager.loadAll();
    }

    // The frame the line leaves in a buffer of internalFormat, RGBA as stored; empty for none
    [[nodiscard]] std::vector<uint8_t> textFrameIn(GLint internalFormat)
    {
        NativeWindow & window = m_connector->window();
        return frameIn(internalFormat, [this, &window] {
            Ui::Gl::GlRender        render([&window] { window.makeCurrent(); }, m_resManager.resPath().fontDir());
            const Ui::font_handle_t font = render.createFont(m_resManager.theme().menuItemFont);
            render.beginFrame(TEXT_FRAME_W, TEXT_FRAME_H);
            glClearColor(TEXT_BACKGROUND.r() / 255.0F,
                         TEXT_BACKGROUND.g() / 255.0F,
                         TEXT_BACKGROUND.b() / 255.0F,
                         1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
            render.drawText(font,
                            "Wiki Page System Info",
                            { .x = 8.0F, .y = 8.0F, .w = TEXT_FRAME_W - 16.0F, .h = 40.0F },
                            TEXT_COLOR,
                            Ui::Res::Type::AlignH::Left,
                            Ui::Res::Type::AlignV::Center,
                            0.0F);
            render.endFrame();
        });
    }

    // The same line as the tooltip draws it, over its opaque box: the text path every popup's
    // renderer shares, a list's and a dialog's too
    [[nodiscard]] std::vector<uint8_t> tooltipFrameIn(GLint internalFormat)
    {
        NativeWindow & window = m_connector->window();
        return frameIn(internalFormat, [this, &window] {
            Ui::Render::Popup::TooltipRenderer tooltip([&window] { window.makeCurrent(); },
                                                       m_resManager,
                                                       "Wiki Page System Info");
            tooltip.resize(TEXT_FRAME_W, TEXT_FRAME_H);
            (void)tooltip.render();
        });
    }

    // What draw leaves in a buffer of internalFormat, RGBA as stored; empty for none
    [[nodiscard]] static std::vector<uint8_t> frameIn(GLint internalFormat, const std::function<void()> & draw)
    {
        GLuint fbo    = 0;
        GLuint fboTex = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &fboTex);
        glBindTexture(GL_TEXTURE_2D, fboTex);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     internalFormat,
                     TEXT_FRAME_W,
                     TEXT_FRAME_H,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);

        std::vector<uint8_t> rgba;
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            draw();
            glFinish();
            // Read as stored, an sRGB buffer's bytes not decoded
            glDisable(GL_FRAMEBUFFER_SRGB);
            rgba.resize(static_cast<std::size_t>(TEXT_FRAME_W) * TEXT_FRAME_H * 4);
            glReadPixels(0, 0, TEXT_FRAME_W, TEXT_FRAME_H, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &fboTex);
        glDeleteFramebuffers(1, &fbo);
        return rgba;
    }

    // Whether pixel idx of rgba is the text's, not the ground
    [[nodiscard]] static bool isDrawn(const std::vector<uint8_t> & rgba, std::size_t idx)
    {
        return std::abs(rgba.at(idx) - TEXT_BACKGROUND.r()) > COLOR_TOLERANCE;
    }
};

// Text over an opaque frame leaves it opaque. Alpha blended as colour leaves every soft edge of
// a glyph part see-through, which a compositor that honours a window's alpha shows as thinner
// text - native Wayland's does, for the main window and every list
TEST_F(TextFrameTest, TextLeavesAnOpaqueFrameOpaque)
{
    const std::vector<uint8_t> rgba = textFrameIn(GL_RGBA8);
    ASSERT_FALSE(rgba.empty()) << "no buffer to draw into";
    int drawn      = 0;
    int seeThrough = 0;
    for (std::size_t idx = 0; idx + 3 < rgba.size(); idx += 4) {
        if (isDrawn(rgba, idx)) {
            ++drawn;
        }
        if (rgba.at(idx + 3) != 255) {
            ++seeThrough;
        }
    }
    EXPECT_GT(drawn, 0) << "no text was drawn";
    EXPECT_EQ(seeThrough, 0) << "of " << drawn << " pixels the text drew";
}

// The same over a popup's opaque box, through the text path every popup's renderer shares: native
// Wayland's thin lists were drawn there
TEST_F(TextFrameTest, PopupTextLeavesAnOpaqueFrameOpaque)
{
    const std::vector<uint8_t> rgba = tooltipFrameIn(GL_RGBA8);
    ASSERT_FALSE(rgba.empty()) << "no buffer to draw into";
    const Ui::Color & ground     = m_resManager.theme().tooltip.bg;
    int               drawn      = 0;
    int               seeThrough = 0;
    for (std::size_t idx = 0; idx + 3 < rgba.size(); idx += 4) {
        if (std::abs(rgba.at(idx) - ground.r()) > COLOR_TOLERANCE) {
            ++drawn;
        }
        if (rgba.at(idx + 3) != 255) {
            ++seeThrough;
        }
    }
    EXPECT_GT(drawn, 0) << "no text was drawn";
    EXPECT_EQ(seeThrough, 0) << "of " << drawn << " pixels the text drew";
}

// The same line on a buffer that encodes sRGB and on one that does not: which a window gets is
// its EGL config's, so text the surface could change would look one way on one window and
// another way on the next - native Wayland's lists against X11's did
TEST_F(TextFrameTest, TextLooksTheSameOnAnySurface)
{
    const std::vector<uint8_t> plain = textFrameIn(GL_RGBA8);
    const std::vector<uint8_t> srgb  = textFrameIn(GL_SRGB8_ALPHA8);
    ASSERT_FALSE(plain.empty()) << "no plain buffer to draw into";
    ASSERT_EQ(srgb.size(), plain.size()) << "no sRGB buffer to draw into";
    int drawn     = 0;
    int differing = 0;
    for (std::size_t idx = 0; idx + 3 < plain.size(); idx += 4) {
        if (isDrawn(plain, idx)) {
            ++drawn;
        }
        for (std::size_t channel = 0; channel < 3; ++channel) {
            if (std::abs(plain.at(idx + channel) - srgb.at(idx + channel)) > SAME_TEXT) {
                ++differing;
                break;
            }
        }
    }
    EXPECT_GT(drawn, 0) << "no text was drawn";
    EXPECT_EQ(differing, 0) << "of " << drawn << " pixels the text drew";
}

} // namespace PureGlUi
