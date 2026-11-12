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

#include "common/time.h"
#include "testconst.h"

#include <atomic>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>

namespace PureGlUi {

// The frames on a thread of their own, waited for FRAMES_MOST. A swap that never returns ends the
// process with a line on stderr, since a teardown would close the display under it - so a test
// calling this runs in a child process of its own, a threadsafe death test. Shared with the host's
inline void runWithin(const std::function<void()> & frames)
{
    std::atomic<bool>           isDone { false };
    std::thread                 worker([&frames, &isDone] {
        frames();
        isDone = true;
    });
    const Common::Time::point_t start = Common::Time::now();
    while (!isDone && Common::Time::now() - start < FRAMES_MOST) {
        Common::Time::sleepFor(POLL_INTERVAL);
    }
    if (!isDone) {
        std::cerr << "a swap never returned in " << FRAMES_MOST.count() << " ms" << std::endl;
        std::_Exit(EXIT_FAILURE);
    }
    worker.join();
}

} // namespace PureGlUi
