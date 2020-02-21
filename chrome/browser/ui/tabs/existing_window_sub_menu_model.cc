// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/vector_icons.h"

namespace {

enum ExistingWindowSubMenuCommand {
  CommandNewWindow = TabStripModel::kMinExistingWindowCommandId,
  // CommandLast and subsequent ids will be used for the list of existing window
  // targets.
  CommandLast
};

}  // namespace

ExistingWindowSubMenuModel::ExistingWindowSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabStripModel* model,
    int context_index)
    : SimpleMenuModel(this),
      parent_delegate_(parent_delegate),
      model_(model),
      context_index_(context_index) {
  // Start command ids after the parent menu's ids to avoid collisions.
  AddItemWithStringId(ExistingWindowSubMenuCommand::CommandNewWindow,
                      IDS_TAB_CXMENU_MOVETOANOTHERNEWWINDOW);
  AddSeparator(ui::NORMAL_SEPARATOR);

  auto window_titles = model->GetExistingWindowsForMoveMenu();

  for (size_t i = 0; i < window_titles.size(); i++) {
    const int command_id = ExistingWindowSubMenuCommand::CommandLast + i;
    if (command_id > TabStripModel::kMaxExistingWindowCommandId)
      break;

    AddItem(command_id, window_titles[i]);
  }
}

ExistingWindowSubMenuModel::~ExistingWindowSubMenuModel() = default;

bool ExistingWindowSubMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id < ExistingWindowSubMenuCommand::CommandLast) {
    return parent_delegate_->GetAcceleratorForCommandId(
        SubMenuCommandToTabStripModelCommand(command_id), accelerator);
  }
  return false;
}

bool ExistingWindowSubMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id < ExistingWindowSubMenuCommand::CommandLast) {
    return parent_delegate_->IsCommandIdChecked(
        SubMenuCommandToTabStripModelCommand(command_id));
  }
  return false;
}

bool ExistingWindowSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (command_id < ExistingWindowSubMenuCommand::CommandLast) {
    return parent_delegate_->IsCommandIdEnabled(
        SubMenuCommandToTabStripModelCommand(command_id));
  }
  return true;
}

void ExistingWindowSubMenuModel::ExecuteCommand(int command_id,
                                                int event_flags) {
  if (command_id < ExistingWindowSubMenuCommand::CommandLast) {
    parent_delegate_->ExecuteCommand(
        SubMenuCommandToTabStripModelCommand(command_id), event_flags);
    return;
  }

  const int browser_index =
      command_id - ExistingWindowSubMenuCommand::CommandLast;
  model_->ExecuteAddToExistingWindowCommand(context_index_, browser_index);
}

// static
bool ExistingWindowSubMenuModel::ShouldShowSubmenu(Profile* profile) {
  return chrome::GetTabbedBrowserCount(profile) > 1;
}

// static
int ExistingWindowSubMenuModel::SubMenuCommandToTabStripModelCommand(
    int command_id) {
  switch (command_id) {
    case ExistingWindowSubMenuCommand::CommandNewWindow:
      return TabStripModel::CommandMoveTabsToNewWindow;
    default:
      NOTREACHED();
      return -1;
  }
}
