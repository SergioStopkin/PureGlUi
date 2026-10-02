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

#include "ui/idkind.h"

#include <cstdint>

namespace PureGlUi {

// The kinds a test numbers what it hands the framework by, as a host does its own
// (Ui::idOf over a kind from Ui::IdKind::HostFirst): the framework uses a host id as
// it is, so an untagged one would pass a layer that dropped the kind byte. A kind
// per role, so a layer that took one for another fails too
enum class TestIdKind : std::uint8_t {
    Row     = 128, // a dock row's
    Surface = 129, // a content surface's
    Tab     = 130, // a tab's
};

static_assert(Ui::isHostId(Ui::idOf(TestIdKind::Row, 0)), "test kinds are host kinds");
static_assert(Ui::isHostId(Ui::idOf(TestIdKind::Tab, 0)), "test kinds are host kinds");

} // namespace PureGlUi
