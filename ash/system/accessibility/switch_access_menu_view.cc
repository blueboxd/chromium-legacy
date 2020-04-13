// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_menu_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/switch_access_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/events/event.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

SwitchAccessMenuView::SwitchAccessMenuView()
    : scroll_down_button_(
          new SwitchAccessMenuButton(this,
                                     kSwitchAccessScrollDownIcon,
                                     IDS_ASH_SWITCH_ACCESS_SCROLL_DOWN)),
      select_button_(new SwitchAccessMenuButton(this,
                                                kSwitchAccessSelectIcon,
                                                IDS_ASH_SWITCH_ACCESS_SELECT)),
      settings_button_(
          new SwitchAccessMenuButton(this,
                                     kSwitchAccessSettingsIcon,
                                     IDS_ASH_SWITCH_ACCESS_SETTINGS)) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedMenuPadding));

  AddChildView(select_button_);
  AddChildView(scroll_down_button_);
  AddChildView(settings_button_);
}

int SwitchAccessMenuView::GetBubbleWidthDip() const {
  // Fixed at 3 menu items, as the menu currently has a max of 3 items per row
  // and will not show with less than 3 items. In the future this will vary with
  // the number of menu items displayed.
  return (3 * SwitchAccessMenuButton::kWidthDip) + (2 * kUnifiedMenuPadding) +
         kUnifiedMenuItemPadding.left() + kUnifiedMenuItemPadding.right();
}

void SwitchAccessMenuView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  // TODO(crbug/973719): Transition to using new back button and menu which
  //                     will be implemented using views/.
}

const char* SwitchAccessMenuView::GetClassName() const {
  return "SwitchAccessMenuView";
}

}  // namespace ash
