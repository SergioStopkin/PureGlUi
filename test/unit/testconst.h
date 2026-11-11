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

// Every constant the unit tests hold, in one place to read and set them

#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace PureGlUi {

// How long a stand-in on the other side - a child process, the portal on a test bus - is given
// to answer, in ms, and how often it is asked meanwhile
constexpr double                    MOST_WAIT_MS = 5000.0;
constexpr std::chrono::milliseconds POLL_STEP { 1 };

// The message bus's method that hands a name out, which the stand-in portal takes its own by
constexpr const char * REQUEST_NAME = "RequestName";

// Where the test bus finds how to start the portal on the first call
constexpr const char * PORTAL_SERVICE_DIR = TEST_DATA_DIR "/dbus";

// Sleeps of one millisecond, enough of them for a median a stray wake-up does not move, and the
// most one may take: well under a Windows timer tick, well over a sleep that wakes when asked
constexpr std::chrono::milliseconds ONE_MS { 1 };
constexpr std::size_t               SLEEPS        = 9;
constexpr double                    MOST_SLEEP_MS = 5.0;

// Every scale an edge has to hold at: the fractional ones are what toPhysEdge is for, and CI
// only ever runs 1x and 1.25x
constexpr std::array SCALES { 1.0F, 1.25F, 1.5F, 1.75F, 2.0F, 2.5F, 3.0F };

// CSS px either way of the window's corner, in half steps: a value off the window, and one
// that is not whole, are both handed to it
constexpr int MOST_HALF_STEPS = 1600;

// A dialog's size in CSS px, the span between its two edges
constexpr Ui::fpx_t DIALOG_WIDTH = 300.0F;

// A tab strip's least tab and its scroll arrow
constexpr Ui::fpx_t MIN_TAB = 100.0F;
constexpr Ui::fpx_t ARROW   = 20.0F;

// One track for every slider test, offset from the origin on both axes so a mapping that
// forgets to subtract the track's own start shows up.
// res states the thumb's length in production, so the test owns it too rather than deriving it
// from the track - that coupling is exactly what moved out
constexpr Ui::Res::Type::bound_t TRACK { 100.0F, 50.0F, 200.0F, 10.0F };
constexpr Ui::fpx_t              THUMB  = 20.0F;
constexpr Ui::fpx_t              TRAVEL = TRACK.w - THUMB;

// One area for every scrollbar test: 200 wide, 100 tall, 400 of content in a 100 viewport, so
// exactly 300 is scrollable and the thumb is a quarter of the track
constexpr Ui::fpx_t AREA_W    = 200.0F;
constexpr Ui::fpx_t AREA_H    = 100.0F;
constexpr Ui::fpx_t CONTENT_H = 400.0F;
constexpr Ui::fpx_t MAX_VALUE = CONTENT_H - AREA_H;

// Two buttons a click counter tells apart, ids it never reads, and the double-click it is set to
constexpr std::uint32_t LMB               = 1;
constexpr std::uint32_t RMB               = 3;
constexpr std::uint32_t CLICK_INTERVAL_MS = 400;
constexpr int           CLICK_DISTANCE_PX = 5;

// What a sequence that is no UTF-8 reads as, and a 3-byte and a 4-byte sequence
constexpr std::uint32_t  REPLACEMENT = 0xFFFDU;
inline const std::string EURO        = "\xE2\x82\xAC";     // U+20AC
inline const std::string EMOJI       = "\xF0\x9F\x98\x80"; // U+1F600

// A toolbar button with a tooltip, and another
constexpr Ui::id_t BUTTON = 5;
constexpr Ui::id_t OTHER  = 6;

// The window a tooltip stands in, and the tooltip's width
constexpr Ui::fpx_t WINDOW_W  = 800.0F;
constexpr Ui::fpx_t WINDOW_H  = 600.0F;
constexpr Ui::fpx_t TOOLTIP_W = 100.0F;

// Every glyph is one unit wide, so "abcd" measures 4 and the arithmetic in the expectations is
// the same a reader can do by hand. The ellipsis is a glyph like any other, so it also costs 1
constexpr Ui::fpx_t         GLYPH = 1.0F;
constexpr Ui::font_handle_t FONT  = 1;

// What a host's res gives the no-handler warning: its icon and the text before the type
inline const std::string WARNING_ICON      = "warning.svg";
inline const std::string NO_HANDLER_PREFIX = "No handler for";

} // namespace PureGlUi
