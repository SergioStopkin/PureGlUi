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

// Every constant the component tests hold, in one place to read and set them

#include "testidkind.h"
#include "ui/color.h"
#include "ui/idkind.h"
#include "ui/type.h"

#include <array>
#include <chrono>

namespace PureGlUi {

// How long the OS is given to act on what it was asked, and the most a change of state takes
constexpr std::chrono::milliseconds SETTLE { 100 };
constexpr std::chrono::milliseconds MOST_WAIT { 5000 };

// The most a window made, a few frames and closed take, many times over: a swap waiting for a
// frame callback that never comes never returns, and running out ends the child process drawing
// them (runWithin). And how often the wait looks
constexpr std::chrono::milliseconds FRAMES_MOST { 1000 };
constexpr std::chrono::milliseconds POLL_INTERVAL { 10 };

// The window a session that ended fullscreen kept, in physical px: off the layout's default and
// over the window's least, so a window back at it is the session's
constexpr int RESTORED_WIDTH  = 1700;
constexpr int RESTORED_HEIGHT = 1100;

// The physical px a child window, as a host's content surface, is made at
constexpr Ui::fpx_t CHILD_WIDTH  = 320.0F;
constexpr Ui::fpx_t CHILD_HEIGHT = 240.0F;

// Passes the loop runs: the ones a test scripts, then several with nothing new, for
// anything the shell still owes the pointer
constexpr int PASSES = 8;

// Physical px a window is moved or resized by: past half a title, so a press read against
// the frame before the change lands off it
constexpr Ui::fpx_t SHIFT = 120.0F;

constexpr const char * NO_WINDOW     = "no window, or no title opens a list here";
constexpr const char * NO_FOCUS      = "the OS gives the app no focus here";
constexpr const char * NO_FOCUS_LOSS = "the OS kept the app active here";
constexpr const char * NO_MINIMIZE   = "AppKit does not minimize the window here";

// The Q key on macOS, kVK_ANSI_Q
constexpr unsigned short MAC_KEY_Q = 12;

// Nothing done to the window round a press
constexpr auto NOTHING = [] { };

// Flip to true to trace where the OS has a popup at each step of putting it on screen
constexpr bool PLACEMENT_DEBUG = false;

// A character's width in the fixed text metrics of a fake render
constexpr Ui::fpx_t GLYPH_WIDTH = 8.0F;

constexpr Ui::fpx_t WINDOW_W = 1600.0F;
constexpr Ui::fpx_t WINDOW_H = 1000.0F;

constexpr Ui::fpx_t VIEW_W = 1200.0F;
constexpr Ui::fpx_t VIEW_H = 800.0F;

constexpr Ui::fpx_t DOCK_TOP              = 100.0F;
constexpr Ui::fpx_t DOCK_EDGE             = 300.0F; // viewport-facing edge of a left dock
constexpr Ui::fpx_t DOCK_WIDTH            = 180.0F;
constexpr Ui::fpx_t SCROLLBAR_DOCK_HEIGHT = 200.0F;
constexpr Ui::fpx_t SLIDER_DOCK_HEIGHT    = 400.0F;
constexpr Ui::id_t  SLIDER_ROW            = Ui::idOf(TestIdKind::Row, 7);

// A row a host projects into a dock
constexpr Ui::id_t PROJECTED_ROW = Ui::idOf(TestIdKind::Row, 8);

// The dock TestDockRowLayout lays its rows out in
constexpr Ui::fpx_t ROW_DOCK_HEIGHT = 300.0F;
constexpr Ui::fpx_t ROW_DOCK_EDGE   = 400.0F;
constexpr Ui::fpx_t ROW_DOCK_WIDTH  = 200.0F;

// Narrower than a row's padding either side, and wide enough for rows yet not
// for a slider track or a deep expander. Dragging a dock shut passes both
constexpr std::array NARROW_WIDTHS { 12.0F, 60.0F };

// How far past its area along x a piece may stand and still be inside
constexpr Ui::fpx_t EPSILON = 0.01F;

constexpr Ui::id_t SURFACE_A       = Ui::idOf(TestIdKind::Surface, 10);
constexpr Ui::id_t SURFACE_B       = Ui::idOf(TestIdKind::Surface, 20);
constexpr Ui::id_t UNKNOWN_SURFACE = Ui::idOf(TestIdKind::Surface, 999);

// A frame text is drawn into, physical px, and its colours: a dark line on a light opaque ground
constexpr int          TEXT_FRAME_W = 320;
constexpr int          TEXT_FRAME_H = 64;
inline const Ui::Color TEXT_BACKGROUND(204, 204, 191);
inline const Ui::Color TEXT_COLOR(16, 48, 16);

// Per channel: how far a pixel may stray from the ground and still read as ground
constexpr int COLOR_TOLERANCE = 16;

// Per channel: the same text drawn twice agrees to rounding
constexpr int SAME_TEXT = 1;

} // namespace PureGlUi
