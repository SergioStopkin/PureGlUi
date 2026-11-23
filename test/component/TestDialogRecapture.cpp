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
 * @file TestDialogRecapture.cpp
 * @brief What stands under an open dialog's corners is read as it opens, and again on the
 *        first main frame after a resize recentres it - never on every main frame, which over
 *        a host's view would read the view and draw the dialog again whole each time
 *
 * Popups as windows of their own, where a recapture asks for the dialog's own frame: the
 * trace of it the render queue keeps. Composited, where a main frame that read the dialog
 * draws it in and its own frame is skipped, what the dialog last drew is read instead: a
 * change it draws must reach it whether a main frame drew with it or not
 */

#include "common/time.h"
#include "popupsaswindows.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/animation/durationof.h"
#include "ui/config.h"
#include "ui/gl/localglew.h"
#include "ui/res/key/element.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/dialog.h"
#include "ui/res/type/dialogtype.h"
#include "ui/shell.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/windowmanager.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace {

constexpr int MOST_SETTLING_FRAMES = 10; // for a dialog just opened to stop asking for frames of its own
constexpr int MAIN_FRAMES          = 5;  // a busy view under it, pulling in a main frame each

// A shell with a dialog open, and the frames a test drives it through
class OpenDialogTest : public PureGlUi::ShellTest {
protected:
    [[nodiscard]] Ui::Window::WindowManager & windowManager() { return m_shell.windowManager(); }

    void openDialog(Ui::Res::Type::DialogType type = Ui::Res::Type::DialogType::Info)
    {
        Ui::Res::Type::dialog_t dialog;
        dialog.type   = type;
        dialog.title  = "recapture";
        dialog.width  = 300.0F;
        dialog.height = 200.0F;
        windowManager().dialog().open(dialog);
        ASSERT_NE(windowManager().dialog().window(), nullptr) << m_read;
    }

    // Whether the dialog's own frame waits in the render queue - what its recapture asks for
    [[nodiscard]] bool isDialogFrameDue()
    {
        const Ui::Window::Popup::DialogWindow * dialog = windowManager().dialog().window();
        return dialog != nullptr && windowManager().renderQueue().pending().contains(dialog->subscribeId());
    }

    // One main frame, with the notify that recaptures what stands on the main window
    void renderMainFrame()
    {
        windowManager().requestMainRender();
        windowManager().renderAll();
    }

    // The window's size handed over again, as a resize hands it
    void resizeInPlace()
    {
        windowManager().onMainWindowResize(windowManager().windowWidth(), windowManager().windowHeight());
    }
};

class DialogRecaptureTest : public OpenDialogTest {
protected:
    PureGlUi::PopupsAsWindows m_popupsAsWindows; // before SetUp starts the shell

    void SetUp() override
    {
        OpenDialogTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        if (Ui::g_config.isCompositing) {
            GTEST_SKIP() << "popups drawn into the main window here: a recapture asks for no frame of the dialog's";
        }
        openDialog();
        for (int frame = 0; frame < MOST_SETTLING_FRAMES && isDialogFrameDue(); ++frame) {
            windowManager().renderAll();
        }
    }
};

// Composited: a Confirm dialog, whose Right moves the keyboard's focus onto a button that fades
// into its hover - a change only the dialog's own frame draws, unless a main frame reads it
class CompositedDialogTest : public OpenDialogTest {
protected:
    Common::Time::span_t m_fade {}; // how long a button's hover takes to land (dialog-button's transition)

    void SetUp() override
    {
        OpenDialogTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        if (!Ui::g_config.isCompositing) {
            GTEST_SKIP() << "popups are windows of their own here: no main frame draws the dialog in";
        }
        m_fade = Ui::Animation::durationOf(m_shell.resManager().transitionsFor(Ui::Res::Key::ElementKey::DialogButton));
        openDialog(Ui::Res::Type::DialogType::Confirm);
        settle(false);
    }

    // Frames until a fade begun now has landed: each with a main frame queued too where
    // `withMainFrame`, as a busy view under the dialog pulls one in, and with the glide's
    // own asked for as the shell's loop asks (keepGliding)
    void settle(bool withMainFrame)
    {
        const Common::Time::point_t until  = Common::Time::now() + (m_fade * 2) + PureGlUi::GLIDE_MARGIN;
        bool                        isLast = false;
        while (!isLast) {
            isLast = Common::Time::now() >= until;
            if (withMainFrame) {
                windowManager().requestMainRender();
            }
            windowManager().keepGliding();
            windowManager().renderAll();
            Common::Time::sleepFor(PureGlUi::GLIDE_MARGIN);
        }
    }

    // What the dialog last drew, read from its own back buffer: composited, its frames are
    // read into the main window and never swapped
    [[nodiscard]] std::vector<std::uint8_t> dialogPixels()
    {
        Ui::Window::Popup::DialogWindow & dialog = *windowManager().dialog().window();
        const int                         width  = Ui::roundToInt(dialog.bound().w);
        const int                         height = Ui::roundToInt(dialog.bound().h);
        std::vector<std::uint8_t>         pixels(static_cast<std::size_t>(width) * height * 4);
        dialog.makeCurrent();
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        mainWindow().makeCurrent();
        return pixels;
    }

    // The keyboard's focus moved onto a button, which asks for the dialog's own frame
    void focusAButton() { windowManager().dialog().keyPress("Right"); }
};

} // namespace

// What the others stand on: open and drawn, the dialog asks for no frame of its own
TEST_F(DialogRecaptureTest, AnOpenDialogSettles)
{
    EXPECT_FALSE(isDialogFrameDue()) << "it keeps asking for frames of its own; " << m_read;
}

// Negative: a main frame with nothing moved leaves it as it was read when it opened
TEST_F(DialogRecaptureTest, AMainFrameLeavesItUnread)
{
    ASSERT_FALSE(isDialogFrameDue()) << m_read;
    renderMainFrame();
    EXPECT_FALSE(isDialogFrameDue()) << m_read;
}

// Negative: nor does a run of them
TEST_F(DialogRecaptureTest, MainFramesInARowLeaveItUnread)
{
    ASSERT_FALSE(isDialogFrameDue()) << m_read;
    for (int frame = 0; frame < MAIN_FRAMES; ++frame) {
        renderMainFrame();
        EXPECT_FALSE(isDialogFrameDue()) << "main frame " << frame << "; " << m_read;
    }
}

// Negative: a resize alone reads nothing - the read waits for the main frame drawn after it
TEST_F(DialogRecaptureTest, AResizeAloneReadsNothing)
{
    ASSERT_FALSE(isDialogFrameDue()) << m_read;
    resizeInPlace();
    EXPECT_FALSE(isDialogFrameDue()) << m_read;
}

TEST_F(DialogRecaptureTest, AResizeHasTheNextMainFrameReadItAgain)
{
    ASSERT_FALSE(isDialogFrameDue()) << m_read;
    resizeInPlace();
    renderMainFrame();
    EXPECT_TRUE(isDialogFrameDue()) << m_read;
}

// Read once: its own frame drawn, the main frame after leaves it again
TEST_F(DialogRecaptureTest, AResizeHasItReadOnce)
{
    resizeInPlace();
    renderMainFrame();
    ASSERT_TRUE(isDialogFrameDue()) << m_read;
    windowManager().renderAll();
    ASSERT_FALSE(isDialogFrameDue()) << m_read;
    renderMainFrame();
    EXPECT_FALSE(isDialogFrameDue()) << m_read;
}

// Two resizes before a frame, as a drag of the window's edge sends them: read once all the same
TEST_F(DialogRecaptureTest, ResizesBeforeAMainFrameHaveItReadOnce)
{
    resizeInPlace();
    resizeInPlace();
    renderMainFrame();
    ASSERT_TRUE(isDialogFrameDue()) << m_read;
    windowManager().renderAll();
    renderMainFrame();
    EXPECT_FALSE(isDialogFrameDue()) << m_read;
}

// A resize after the read is read again: each recentring, not only the first
TEST_F(DialogRecaptureTest, EveryResizeHasItReadAgain)
{
    for (int resize = 0; resize < 2; ++resize) {
        resizeInPlace();
        renderMainFrame();
        EXPECT_TRUE(isDialogFrameDue()) << "resize " << resize << "; " << m_read;
        windowManager().renderAll();
    }
}

// A smaller window recentres it in the area left, at its own size
TEST_F(DialogRecaptureTest, AResizeRecentresIt)
{
    const Ui::Res::Type::bound_t before = windowManager().dialog().window()->bound();
    windowManager().onMainWindowResize(windowManager().windowWidth() - 200.0F, windowManager().windowHeight() - 100.0F);
    const Ui::Res::Type::bound_t after = windowManager().dialog().window()->bound();
    EXPECT_LT(after.x, before.x) << m_read;
    EXPECT_LT(after.y, before.y) << m_read;
    EXPECT_FLOAT_EQ(after.w, before.w) << m_read;
    EXPECT_FLOAT_EQ(after.h, before.h) << m_read;
}

// Negative: a resize the dialog closed after is not read into the next one, read as it opened
TEST_F(DialogRecaptureTest, ADialogOpenedAfterAResizedOneClosedIsNotReadAgain)
{
    resizeInPlace();
    windowManager().dialog().close();
    openDialog();
    renderMainFrame();
    EXPECT_FALSE(isDialogFrameDue()) << m_read;
}

// Negative: a resize with no dialog open leaves the one opened after it as it was read
TEST_F(DialogRecaptureTest, AResizeWithNoDialogLeavesTheNextOneUnread)
{
    windowManager().dialog().close();
    resizeInPlace();
    openDialog();
    renderMainFrame();
    EXPECT_FALSE(isDialogFrameDue()) << m_read;
}

// What the others stand on: its own frames alone draw a change it makes, and a focused button
// looks other than one at rest
TEST_F(CompositedDialogTest, ItsOwnFramesDrawWhatChanged)
{
    const std::vector<std::uint8_t> before = dialogPixels();
    focusAButton();
    settle(false);
    EXPECT_NE(dialogPixels(), before) << "the focused button never drew; " << m_read;
}

// A main frame with each of its own, the dialog unmoved: the main frame never read it, so
// its own frame is not skipped for one that did
TEST_F(CompositedDialogTest, ItsOwnFramesDrawWhatChangedWithAMainFrameEach)
{
    const std::vector<std::uint8_t> before = dialogPixels();
    focusAButton();
    settle(true);
    EXPECT_NE(dialogPixels(), before) << "its own frames were skipped for main frames that never read it; " << m_read;
}

// A main frame that read it, after a resize, counts for that frame alone: the dialog's own
// frames after it are drawn
TEST_F(CompositedDialogTest, AReadOnAMainFrameLeavesItsLaterOwnFramesDrawn)
{
    resizeInPlace();
    renderMainFrame();
    const std::vector<std::uint8_t> before = dialogPixels();
    focusAButton();
    settle(false);
    EXPECT_NE(dialogPixels(), before) << "a read left its own frames skipped after it; " << m_read;
}

// The same, a main frame with each of its own after that read
TEST_F(CompositedDialogTest, AReadOnAMainFrameLeavesItsLaterOwnFramesDrawnWithAMainFrameEach)
{
    resizeInPlace();
    renderMainFrame();
    const std::vector<std::uint8_t> before = dialogPixels();
    focusAButton();
    settle(true);
    EXPECT_NE(dialogPixels(), before) << "a read left its own frames skipped after it; " << m_read;
}
