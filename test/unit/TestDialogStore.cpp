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
 * @file TestDialogStore.cpp
 * @brief Unit tests for Ui::Res::Store::DialogStore. Loads res/dialog.json
 *        (TEST_RES_DIR), verifies each dialog type resolves a non-empty button
 *        row from the shared button-label table, that exactly one primary exists
 *        per multi-button type, and that an unloaded store returns the empty
 *        config.
 */

#include "ui/res/respath.h"
#include "ui/res/store/dialogstore.h"
#include "ui/res/type/dialog.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using Ui::Res::ResPath;
using Ui::Res::Store::DialogStore;
using Ui::Res::Type::DialogType;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }

int primaryCount(const Ui::Res::Type::dialog_type_config_t & config)
{
    int count = 0;
    for (const auto & button : config.buttons) {
        if (button.primary) {
            ++count;
        }
    }
    return count;
}
} // namespace

TEST(DialogStore, UnloadedReturnsEmptyConfig)
{
    const DialogStore store;
    EXPECT_TRUE(store.dialogTypeConfig(DialogType::Info).buttons.empty());
}

TEST(DialogStore, LoadedTypesHaveButtons)
{
    DialogStore store;
    store.load(resPath().dialogFile());

    // Every shipped dialog type resolves at least one button.
    const std::vector<DialogType> types { DialogType::Info,
                                          DialogType::InfoLink,
                                          DialogType::Confirm,
                                          DialogType::SaveConfirm,
                                          DialogType::Warning };
    for (const DialogType type : types) {
        EXPECT_FALSE(store.dialogTypeConfig(type).buttons.empty())
        << "type " << static_cast<int>(type) << " has no buttons";
    }
}

TEST(DialogStore, ConfirmHasChoiceAndSinglePrimary)
{
    DialogStore store;
    store.load(resPath().dialogFile());

    const auto & confirm = store.dialogTypeConfig(DialogType::Confirm);
    EXPECT_GE(confirm.buttons.size(), 2U); // a confirm dialog offers a choice
    EXPECT_EQ(primaryCount(confirm), 1);   // exactly one default button

    // Button labels are resolved (non-empty locale keys), not left blank.
    for (const auto & button : confirm.buttons) {
        EXPECT_FALSE(button.label.empty());
    }
}
