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

#include "ui/intent.h"

#include <vector>

namespace Ui {

// Outcome of Context::handleEvent: whether the main UI needs a repaint, plus the
// intents the host must execute (in order). isDirty alone (no intents) means a
// fw-internal change happened (hover, tab scroll, menu highlight).
struct alignas(32) result_t final {
    bool                  isDirty = false;
    std::vector<intent_t> intents;

    bool operator==(const result_t &) const = default;
};

} // namespace Ui
