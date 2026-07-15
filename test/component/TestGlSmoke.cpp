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

#include "ui/gl/glrender.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/pubsub/subscribe.h"
#include "ui/render/uirenderer.h"
#include "ui/res/resmanager.h"
#include "ui/window/connector.h"
#include "ui/window/nativewindow.h"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

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

} // namespace PureGlUi
