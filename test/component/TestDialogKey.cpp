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
 * @file TestDialogKey.cpp
 * @brief A dialog's key through the shell's own loop: Escape shows the close button hovered,
 *        then pressed, each step as long as dialog-close's transition in animation.json runs,
 *        and only then closes the dialog - once
 */

#include "common/time.h"
#include "menubarloop.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/animation/durationof.h"
#include "ui/res/key/element.h"
#include "ui/res/type/dialog.h"
#include "ui/shell.h"

#include <gtest/gtest.h>

namespace {

class DialogKeyTest : public PureGlUi::ShellTest {
protected:
    int m_closes = 0; // each close the dialog reported

    void SetUp() override
    {
        ShellTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        m_shell.windowManager().dialog().setOnClose([this] { ++m_closes; });
    }
};

} // namespace

TEST_F(DialogKeyTest, EscapeClosesOnceAfterItsSteps)
{
    const Common::Time::span_t step = Ui::Animation::durationOf(
        m_shell.resManager().transitionsFor(Ui::Res::Key::ElementKey::DialogClose));
    ASSERT_GT(step, Common::Time::span_t::zero()) << "the shipped dialog-close has no transition";
    bool isOpenAfterKey = false;
    PureGlUi::runPasses(
        [this, &step, &isOpenAfterKey](int pass) {
            if (pass == 0) {
                Ui::Res::Type::dialog_t dialog;
                dialog.title  = "keys";
                dialog.width  = 300.0F;
                dialog.height = 200.0F;
                m_shell.windowManager().dialog().open(dialog);
                m_shell.windowManager().dialog().keyPress("Escape");
                isOpenAfterKey = m_shell.windowManager().dialog().isOpen();
            } else if (pass == 1) {
                // Both steps run out, hovered then pressed: the next frame closes it
                Common::Time::sleepFor((step * 2) + PureGlUi::GLIDE_MARGIN);
            }
        },
        m_shell);
    EXPECT_TRUE(isOpenAfterKey) << "the key closed it before its steps; " << m_read;
    EXPECT_FALSE(m_shell.windowManager().dialog().isOpen()) << m_read;
    EXPECT_EQ(m_closes, 1) << m_read;
}
