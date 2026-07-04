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

#include "common/noncopyable.h"
#include "ui/type.h"

#include <set>

namespace Ui::Window {

class RenderQueue final : private Common::NonCopyable {
    std::set<id_t> m_pending;

public:
    RenderQueue()  = default;
    ~RenderQueue() = default;

    void request(id_t windowId) { m_pending.insert(windowId); }

    [[nodiscard]] const std::set<id_t> & pending() const { return m_pending; }

    [[nodiscard]] bool hasPending() const { return !m_pending.empty(); }

    void clear() { m_pending.clear(); }
};

} // namespace Ui::Window
