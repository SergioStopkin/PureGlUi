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
 * @file TestMenuDisable.cpp
 * @brief Exhaustive guard for Ui::Res::Store::disableUnhandled - the "no handler
 *        means no dead click" pass, in both overloads. The menu_t one recursively
 *        collapses dead nodes: positive/negative cases across menu items
 *        (leaves), submenus, and top-menu entries, plus nesting, separators,
 *        pre-disabled nodes, and degenerate inputs. The button_t one is flat (no
 *        collapse step). Pure C++ - no GL/X11, builds synthetic values directly.
 */

#include "ui/res/store/menudisable.h"
#include "ui/res/type/button.h"
#include "ui/res/type/menu.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using Ui::Res::Store::disableUnhandled;
using Ui::Res::Type::menu_t;

// --- builders ---------------------------------------------------------------

// Leaf bound to an actionKey (Cut/Reload/...): enabled iff the key is handled.
menu_t action(std::string key)
{
    menu_t node;
    node.actionKey = std::move(key);
    return node;
}

// Leaf that opens a dialog (title set): functional without a handler.
menu_t dialog(std::string key)
{
    menu_t node;
    node.actionKey    = std::move(key);
    node.dialog.title = "DialogTitle";
    return node;
}

menu_t separator()
{
    menu_t node;
    node.separator = true;
    return node;
}

// Submenu / top-menu node: identified solely by having children.
menu_t parent(std::vector<menu_t> children)
{
    menu_t node;
    node.items = std::move(children);
    return node;
}

// Predicate: only the listed keys report as handled.
Ui::predicate_fn_t handled(std::set<std::string> keys)
{
    return [keys = std::move(keys)](const std::string & key) { return keys.count(key) > 0; };
}

Ui::predicate_fn_t noneHandled() { return handled({}); }

// Toolbar button bound to an actionKey.
Ui::Res::Type::button_t button(std::string key)
{
    Ui::Res::Type::button_t btn;
    btn.actionKey = std::move(key);
    return btn;
}

} // namespace

// === Menu items (leaves) ====================================================

TEST(MenuDisable, LeafHandledStaysEnabled)
{
    std::vector<menu_t> menus = { action("Reload") };
    disableUnhandled(menus, handled({ "Reload" }));
    EXPECT_TRUE(menus[0].enabled);
}

TEST(MenuDisable, LeafUnhandledIsDisabled)
{
    std::vector<menu_t> menus = { action("Reload") };
    disableUnhandled(menus, noneHandled());
    EXPECT_FALSE(menus[0].enabled);
}

TEST(MenuDisable, LeafWithDialogStaysEnabledEvenWhenUnhandled)
{
    std::vector<menu_t> menus = { dialog("About") };
    disableUnhandled(menus, noneHandled());
    EXPECT_TRUE(menus[0].enabled);
}

TEST(MenuDisable, LeafWithoutActionKeyStaysEnabled)
{
    std::vector<menu_t> menus = { menu_t {} }; // no actionKey, no dialog
    disableUnhandled(menus, noneHandled());
    EXPECT_TRUE(menus[0].enabled);
}

// === Submenus (parents) =====================================================

TEST(MenuDisable, SubmenuAllUnhandledCollapsesParentAndChildren)
{
    std::vector<menu_t> menus = { parent({ action("Cut"), action("Copy") }) };
    disableUnhandled(menus, noneHandled());
    EXPECT_FALSE(menus[0].enabled);
    EXPECT_FALSE(menus[0].items[0].enabled);
    EXPECT_FALSE(menus[0].items[1].enabled);
}

TEST(MenuDisable, SubmenuOneHandledKeepsParentEnabled)
{
    std::vector<menu_t> menus = { parent({ action("Cut"), action("Copy") }) };
    disableUnhandled(menus, handled({ "Copy" }));
    EXPECT_TRUE(menus[0].enabled);
    EXPECT_FALSE(menus[0].items[0].enabled); // Cut unhandled
    EXPECT_TRUE(menus[0].items[1].enabled);  // Copy handled
}

TEST(MenuDisable, SubmenuAllHandledStaysEnabled)
{
    std::vector<menu_t> menus = { parent({ action("Cut"), action("Copy") }) };
    disableUnhandled(menus, handled({ "Cut", "Copy" }));
    EXPECT_TRUE(menus[0].enabled);
    EXPECT_TRUE(menus[0].items[0].enabled);
    EXPECT_TRUE(menus[0].items[1].enabled);
}

TEST(MenuDisable, SubmenuWithDialogChildKeepsParentEnabled)
{
    std::vector<menu_t> menus = { parent({ action("Cut"), dialog("About") }) };
    disableUnhandled(menus, noneHandled());
    EXPECT_TRUE(menus[0].enabled);           // the dialog child keeps it alive
    EXPECT_FALSE(menus[0].items[0].enabled); // Cut unhandled
    EXPECT_TRUE(menus[0].items[1].enabled);  // dialog item
}

TEST(MenuDisable, SubmenuOfOnlySeparatorsCollapses)
{
    std::vector<menu_t> menus = { parent({ separator(), separator() }) };
    disableUnhandled(menus, noneHandled());
    EXPECT_FALSE(menus[0].enabled);
}

// === Separators don't count as enabled children =============================

TEST(MenuDisable, SeparatorDoesNotKeepParentEnabled)
{
    std::vector<menu_t> menus = { parent({ separator(), action("Cut") }) };
    disableUnhandled(menus, noneHandled());
    EXPECT_FALSE(menus[0].enabled); // separator ignored; Cut unhandled -> collapse
}

TEST(MenuDisable, SeparatorWithHandledLeafKeepsParentEnabled)
{
    std::vector<menu_t> menus = { parent({ separator(), action("Cut") }) };
    disableUnhandled(menus, handled({ "Cut" }));
    EXPECT_TRUE(menus[0].enabled);
}

// === Top-menu entries (top-level parents) ===================================

TEST(MenuDisable, TopMenusFileEnabledEditDisabled)
{
    std::vector<menu_t> menus = {
        parent({ action("OpenFile"), action("ExitApp") }),         // File
        parent({ action("Cut"), action("Copy"), action("Paste") }) // Edit
    };
    disableUnhandled(menus, handled({ "OpenFile", "ExitApp" }));
    EXPECT_TRUE(menus[0].enabled);  // File: all handled
    EXPECT_FALSE(menus[1].enabled); // Edit: none handled -> top menu collapses
}

// === Nested submenus (recursion, bottom-up) =================================

TEST(MenuDisable, NestedAllUnhandledCollapsesBottomUp)
{
    // top -> submenu -> [leaf, leaf]
    std::vector<menu_t> menus = { parent({ parent({ action("Shaded"), action("Realistic") }) }) };
    disableUnhandled(menus, noneHandled());
    EXPECT_FALSE(menus[0].enabled);                   // top
    EXPECT_FALSE(menus[0].items[0].enabled);          // submenu
    EXPECT_FALSE(menus[0].items[0].items[0].enabled); // Shaded
    EXPECT_FALSE(menus[0].items[0].items[1].enabled); // Realistic
}

TEST(MenuDisable, NestedDeepHandledKeepsAncestorsEnabled)
{
    std::vector<menu_t> menus = { parent({ parent({ action("Shaded"), action("Realistic") }) }) };
    disableUnhandled(menus, handled({ "Realistic" }));
    EXPECT_TRUE(menus[0].enabled);                    // top alive via deep handled leaf
    EXPECT_TRUE(menus[0].items[0].enabled);           // submenu alive
    EXPECT_FALSE(menus[0].items[0].items[0].enabled); // Shaded
    EXPECT_TRUE(menus[0].items[0].items[1].enabled);  // Realistic
}

TEST(MenuDisable, NestedDeadSubmenuButHandledSiblingKeepsTopEnabled)
{
    // top -> [ submenu(all unhandled), handled leaf ]
    std::vector<menu_t> menus = { parent({ parent({ action("Shaded") }), action("Reload") }) };
    disableUnhandled(menus, handled({ "Reload" }));
    EXPECT_TRUE(menus[0].enabled);           // top alive via Reload
    EXPECT_FALSE(menus[0].items[0].enabled); // dead submenu collapsed
    EXPECT_TRUE(menus[0].items[1].enabled);  // Reload
}

// === Never re-enables; respects pre-disabled (host capability gating) =======

TEST(MenuDisable, NeverReEnablesPreDisabledHandledLeaf)
{
    menu_t copy               = action("Copy");
    copy.enabled              = false; // host-gated off, even though handled
    std::vector<menu_t> menus = { parent({ copy, action("Cut") }) };
    disableUnhandled(menus, handled({ "Copy", "Cut" }));
    EXPECT_FALSE(menus[0].items[0].enabled); // stays disabled (never re-enabled)
    EXPECT_TRUE(menus[0].items[1].enabled);
    EXPECT_TRUE(menus[0].enabled); // alive via Cut
}

TEST(MenuDisable, AllChildrenPreDisabledCollapsesParent)
{
    menu_t a                  = action("A");
    menu_t b                  = action("B");
    a.enabled                 = false;
    b.enabled                 = false;
    std::vector<menu_t> menus = { parent({ a, b }) };
    disableUnhandled(menus, handled({ "A", "B" })); // handled, but already off
    EXPECT_FALSE(menus[0].enabled);                 // no enabled child -> collapse
}

// === Degenerate / "impossible" inputs (document behavior, no crash) =========

TEST(MenuDisable, EmptyMenusVectorIsNoOp)
{
    std::vector<menu_t> menus;
    disableUnhandled(menus, noneHandled());
    EXPECT_TRUE(menus.empty());
}

TEST(MenuDisable, EmptyTopMenuStaysEnabled)
{
    // A node with no items and no actionKey is neither a disable-able leaf nor a
    // collapse-able parent, so it is left untouched.
    std::vector<menu_t> menus = { parent({}) };
    disableUnhandled(menus, noneHandled());
    EXPECT_TRUE(menus[0].enabled);
}

TEST(MenuDisable, MisconfiguredEmptySubmenuStaysEnabled)
{
    // submenu key declared but auto-population produced no children: not a leaf
    // (submenu set) and not a parent (no items) -> untouched.
    menu_t node;
    node.submenu              = "ambience";
    std::vector<menu_t> menus = { node };
    disableUnhandled(menus, noneHandled());
    EXPECT_TRUE(menus[0].enabled);
}

// === Toolbar buttons (flat, no collapse step) ===============================

TEST(ButtonDisable, HandledStaysEnabled)
{
    std::vector<Ui::Res::Type::button_t> buttons = { button("OpenFile") };
    disableUnhandled(buttons, handled({ "OpenFile" }));
    EXPECT_TRUE(buttons[0].enabled);
}

TEST(ButtonDisable, UnhandledIsDisabled)
{
    std::vector<Ui::Res::Type::button_t> buttons = { button("Custom1") };
    disableUnhandled(buttons, noneHandled());
    EXPECT_FALSE(buttons[0].enabled);
}

TEST(ButtonDisable, WithoutActionKeyStaysEnabled)
{
    // Nothing to dispatch, so nothing to judge - left alone, like a menu node
    // with no actionKey.
    std::vector<Ui::Res::Type::button_t> buttons = { Ui::Res::Type::button_t {} };
    disableUnhandled(buttons, noneHandled());
    EXPECT_TRUE(buttons[0].enabled);
}

TEST(ButtonDisable, JudgedIndependentlyOfEachOther)
{
    std::vector<Ui::Res::Type::button_t> buttons = { button("OpenFile"), button("Custom1"), button("Reload") };
    disableUnhandled(buttons, handled({ "OpenFile", "Reload" }));
    EXPECT_TRUE(buttons[0].enabled);
    EXPECT_FALSE(buttons[1].enabled);
    EXPECT_TRUE(buttons[2].enabled);
}

TEST(ButtonDisable, NeverReEnablesAResDisabledButton)
{
    // Only ever sets enabled=false: a button switched off in res JSON stays off
    // even when its key does have a handler.
    Ui::Res::Type::button_t off                  = button("OpenFile");
    off.enabled                                  = false;
    std::vector<Ui::Res::Type::button_t> buttons = { off };
    disableUnhandled(buttons, handled({ "OpenFile" }));
    EXPECT_FALSE(buttons[0].enabled);
}

TEST(ButtonDisable, EmptyVectorIsNoOp)
{
    std::vector<Ui::Res::Type::button_t> buttons;
    disableUnhandled(buttons, noneHandled());
    EXPECT_TRUE(buttons.empty());
}
