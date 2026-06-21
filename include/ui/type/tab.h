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

#include "ui/type.h"

#include <string>

namespace Ui::Type {

// Lightweight tab view model the chrome renders. A one-way projection of the
// host's authoritative workspace state (file path, MRU, session stay host-side);
// the host resolves any atomics into these plain values on the main thread.
struct alignas(64) tab_t final {
    id_t        id = INVALID_ID;    // stable identity, host-assigned
    std::string label;              // display text
    bool        isActive   = false; // currently focused tab
    bool        hasContent = false; // has a document (controls close button)
    bool        isLoading  = false; // content load in progress
    int         progress   = 0;     // loading sector index for the tab spinner
};

} // namespace Ui::Type
