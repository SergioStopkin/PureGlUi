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

#include "ui/res/localemanager.h"
#include "ui/res/respath.h"
#include "ui/res/store/iconstore.h"
#include "ui/res/store/layoutstore.h"
#include "ui/res/store/menustore.h"
#include "ui/res/store/themestore.h"

// Not `Test`: inside TEST() bodies unqualified `Test::` resolves to the gtest
// base class ::testing::Test, shadowing a namespace of that name.
namespace TestSupport {

// MenuStore with default-constructed collaborators (nothing loaded), for unit
// tests that drive parseMenuItem / the enable-disable API directly.
struct alignas(128) menu_store_fixture_t final {
    Ui::Res::LocaleManager      locale;
    Ui::Res::Store::IconStore   icons;
    Ui::Res::Store::ThemeStore  theme;
    Ui::Res::Store::LayoutStore layout;
    Ui::Res::ResPath            resPath;
    Ui::Res::Store::MenuStore   store { locale, icons, theme, layout, resPath };
};

} // namespace TestSupport
