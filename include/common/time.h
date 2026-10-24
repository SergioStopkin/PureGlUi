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

#if defined(_WIN32)
#include <ratio>
#include <windows.h>
#endif

#include <chrono>
#include <thread>

namespace Common {

// The one clock everything reads and sleeps on: every moment comes from now(), every
// span is measured between two of them, and every wait ends at one, so the clock
// changes in one place - here
class Time final {
    using source_t = std::chrono::steady_clock;

public:
    using point_t = source_t::time_point;
    using span_t  = source_t::duration;

    [[nodiscard]] static point_t now() { return source_t::now(); }

    // Milliseconds from `from` to `to`, fractional
    [[nodiscard]] static double msBetween(point_t from, point_t to)
    {
        return std::chrono::duration<double, std::milli>(to - from).count();
    }

    // Milliseconds from `start` to now
    [[nodiscard]] static double msSince(point_t start) { return msBetween(start, now()); }

    // Sleeps `span`, to a fraction of a millisecond on every platform
    static void sleepFor(span_t span)
    {
        if (span <= span_t::zero()) {
            return;
        }
#if defined(_WIN32)
        // A plain sleep there wakes on the system timer's tick, 15.6 ms unless some
        // process asked for a finer one; a high-resolution waitable timer does not
        using ticks_t       = std::chrono::duration<LONGLONG, std::ratio<1, 10'000'000>>;
        HANDLE        timer = CreateWaitableTimerExW(nullptr,
                                              nullptr,
                                              CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                              TIMER_ALL_ACCESS);
        LARGE_INTEGER due {};
        // Negative: relative to now, in 100 ns units
        due.QuadPart = -std::chrono::duration_cast<ticks_t>(span).count();
        if (timer != nullptr && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) != 0) {
            WaitForSingleObject(timer, INFINITE);
        } else {
            std::this_thread::sleep_for(span);
        }
        if (timer != nullptr) {
            CloseHandle(timer);
        }
#else
        std::this_thread::sleep_for(span);
#endif
    }

    // Sleeps until `point`, at once when it has passed
    static void sleepUntil(point_t point) { sleepFor(point - now()); }
};

} // namespace Common
