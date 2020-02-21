// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_

#include <stddef.h>

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

class Profile;
class TabStripModel;

class ExistingWindowSubMenuModel : public ui::SimpleMenuModel,
                                   ui::SimpleMenuModel::Delegate {
 public:
  ExistingWindowSubMenuModel(ui::SimpleMenuModel::Delegate* parent_delegate,
                             TabStripModel* model,
                             int context_index);
  ~ExistingWindowSubMenuModel() override;

  // ui::SimpleMenuModel
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Whether the submenu should be shown in the provided context. True iff
  // the submenu would show at least one window. Does not assume ownership of
  // |model|; |model| must outlive this instance.
  static bool ShouldShowSubmenu(Profile* profile);

 private:
  static int SubMenuCommandToTabStripModelCommand(int command_id);
  ui::SimpleMenuModel::Delegate* parent_delegate_;
  TabStripModel* model_;
  int context_index_;

  DISALLOW_COPY_AND_ASSIGN(ExistingWindowSubMenuModel);
};

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_WINDOW_SUB_MENU_MODEL_H_
