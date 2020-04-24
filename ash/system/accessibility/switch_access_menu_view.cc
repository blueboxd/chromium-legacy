// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_menu_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/switch_access_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr char kUniqueId[] = "switch_access_menu_view";

struct ButtonInfo {
  const char* name;
  const gfx::VectorIcon* icon;
  int label_id;
};

// These strings must match the values of
// accessibility_private::SwitchAccessMenuAction.
const ButtonInfo kMenuButtonDetails[] = {
    {"decrement", &kSwitchAccessDecrementIcon, IDS_ASH_SWITCH_ACCESS_DECREMENT},
    {"dictation", &kDictationOnNewuiIcon, IDS_ASH_SWITCH_ACCESS_DICTATION},
    {"increment", &kSwitchAccessIncrementIcon, IDS_ASH_SWITCH_ACCESS_INCREMENT},
    {"keyboard", &kSwitchAccessKeyboardIcon, IDS_ASH_SWITCH_ACCESS_KEYBOARD},
    {"scrollDown", &kSwitchAccessScrollDownIcon,
     IDS_ASH_SWITCH_ACCESS_SCROLL_DOWN},
    {"scrollLeft", &kSwitchAccessScrollLeftIcon,
     IDS_ASH_SWITCH_ACCESS_SCROLL_LEFT},
    {"scrollRight", &kSwitchAccessScrollRightIcon,
     IDS_ASH_SWITCH_ACCESS_SCROLL_RIGHT},
    {"scrollUp", &kSwitchAccessScrollUpIcon, IDS_ASH_SWITCH_ACCESS_SCROLL_UP},
    {"select", &kSwitchAccessSelectIcon, IDS_ASH_SWITCH_ACCESS_SELECT},
    {"settings", &kSwitchAccessSettingsIcon, IDS_ASH_SWITCH_ACCESS_SETTINGS},
};

}  // namespace

SwitchAccessMenuView::SwitchAccessMenuView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedMenuPadding));
}

SwitchAccessMenuView::~SwitchAccessMenuView() {}

void SwitchAccessMenuView::SetActions(std::vector<std::string> actions) {
  RemoveAllChildViews(/*delete_children=*/true);

  for (std::string action : actions) {
    for (ButtonInfo info : kMenuButtonDetails) {
      if (action == info.name) {
        AddChildView(
            new SwitchAccessMenuButton(info.name, *info.icon, info.label_id));
        break;
      }
    }
  }
}

int SwitchAccessMenuView::GetBubbleWidthDip() const {
  // Fixed at 3 menu items, as the menu currently has a max of 3 items per row
  // and will not show with less than 3 items. In the future this will vary with
  // the number of menu items displayed.
  return (3 * SwitchAccessMenuButton::kWidthDip) + (2 * kUnifiedMenuPadding) +
         kUnifiedMenuItemPadding.left() + kUnifiedMenuItemPadding.right();
}

void SwitchAccessMenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kMenu;
  node_data->html_attributes.push_back(std::make_pair("id", kUniqueId));
}

const char* SwitchAccessMenuView::GetClassName() const {
  return "SwitchAccessMenuView";
}

}  // namespace ash
