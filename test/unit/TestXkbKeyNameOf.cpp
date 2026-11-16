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
 * @file TestXkbKeyNameOf.cpp
 * @brief Unit tests for ui/window/platform/xkbkeynameof.h, the name native Wayland gives a key,
 *        on a US keymap with keys held as a user holds them: as X11 names it, from its level
 *        without Shift - Shift+Tab is Tab, Shift+a is a - and each Ctrl by its side. Built in a
 *        native Wayland build alone, where xkbcommon is linked
 */

#include "common/cfree.h"
#include "ui/window/platform/xkbkeynameof.h"

#include <gtest/gtest.h>
#include <linux/input-event-codes.h>
#include <string>
#include <xkbcommon/xkbcommon.h>

using Ui::Window::Platform::xkbKeyNameOf;

namespace {

using context_t = Common::c_ptr_t<xkb_context, xkb_context_unref>;
using keymap_t  = Common::c_ptr_t<xkb_keymap, xkb_keymap_unref>;
using state_t   = Common::c_ptr_t<xkb_state, xkb_state_unref>;

// An evdev key code as XKB numbers it
constexpr xkb_keycode_t xkbOf(int evdev) { return static_cast<xkb_keycode_t>(evdev + 8); }

// A US keymap and its state, nothing held yet; skipped where the system has no XKB data
class XkbKeyNameOfTest : public ::testing::Test {
protected:
    context_t m_context { xkb_context_new(XKB_CONTEXT_NO_FLAGS) };
    keymap_t  m_keymap;
    state_t   m_state;

    void SetUp() override
    {
        const xkb_rule_names names { .rules = "evdev", .model = "pc105", .layout = "us", .variant = "", .options = "" };
        if (m_context) {
            m_keymap.reset(xkb_keymap_new_from_names(m_context.get(), &names, XKB_KEYMAP_COMPILE_NO_FLAGS));
        }
        if (!m_keymap) {
            GTEST_SKIP() << "no XKB keymap data here";
        }
        m_state.reset(xkb_state_new(m_keymap.get()));
    }

    void hold(int evdev) { xkb_state_update_key(m_state.get(), xkbOf(evdev), XKB_KEY_DOWN); }

    [[nodiscard]] std::string nameOf(int evdev) { return xkbKeyNameOf(m_state.get(), m_keymap.get(), xkbOf(evdev)); }
};

} // namespace

TEST_F(XkbKeyNameOfTest, ShiftTabIsTab)
{
    hold(KEY_LEFTSHIFT);
    EXPECT_EQ(nameOf(KEY_TAB), "Tab");
}

TEST_F(XkbKeyNameOfTest, ShiftAIsA)
{
    hold(KEY_LEFTSHIFT);
    EXPECT_EQ(nameOf(KEY_A), "a");
}

// Each Ctrl by its side, as the Ctrl+Tab list reads it going up
TEST_F(XkbKeyNameOfTest, EachCtrlByItsSide)
{
    EXPECT_EQ(nameOf(KEY_LEFTCTRL), "Control_L");
    EXPECT_EQ(nameOf(KEY_RIGHTCTRL), "Control_R");
}
