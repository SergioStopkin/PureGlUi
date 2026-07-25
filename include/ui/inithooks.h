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

namespace Ui {

/**
 * @brief Host steps Shell weaves into the load cycle and the init spine.
 *
 * Handed to Shell::initialize() once and kept, so every later reload replays the
 * same steps in the same order - that is what stops startup and reload chrome
 * from diverging. Each hook is optional ({} = skip), so a standalone shell
 * passes none.
 *
 * Named by PURPOSE, not by position in the spine: the first two run on both
 * startup and every reload, so a positional name would be wrong on one path.
 * Restoring persisted state needs no hook - a host does that before initialize().
 */
struct alignas(128) init_hooks_t final {
    Ui::task_fn_t loadDomainResources; // host res the fw knows nothing about; startup + every reload
    Ui::task_fn_t gateFeatures;        // disable what this machine/build cannot do; startup + every reload
    Ui::task_fn_t afterWindowCreated;  // one-time: window + GL context exist, renderer does not
    Ui::task_fn_t afterInit;           // one-time: the spine is complete, everything is live
    Ui::task_fn_t afterReload;         // once per reload, after the chrome is back up (e.g. flush session)
};

} // namespace Ui
