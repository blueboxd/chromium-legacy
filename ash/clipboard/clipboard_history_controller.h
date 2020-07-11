// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ui/base/models/simple_menu_model.h"

namespace ui {
class ClipboardData;
}  // namespace ui

namespace ash {
class ClipboardHistory;
class ClipboardHistoryAcceleratorTarget;
class ClipboardHistoryMenuModelAdapter;

// Shows a menu with the last few things saved in the clipboard when the
// keyboard shortcut is pressed.
class ClipboardHistoryController {
 public:
  ClipboardHistoryController();
  ClipboardHistoryController(const ClipboardHistoryController&) = delete;
  ClipboardHistoryController& operator=(const ClipboardHistoryController&) =
      delete;
  ~ClipboardHistoryController();

  void Init();

  // Whether a menu can be shown.
  bool CanShowMenu() const;

  // Shows a menu with the last few items copied. Executing one of the menu
  // options results in that item being pasted into the active window.
  void ShowMenu();

  // Called when a menu option is selected.
  void MenuOptionSelected(int index);

  ClipboardHistory* clipboard_history() { return clipboard_history_.get(); }

 private:
  // The menu being shown.
  std::unique_ptr<ClipboardHistoryMenuModelAdapter> context_menu_;
  // Used to keep track of what is being copied to the clipboard.
  std::unique_ptr<ClipboardHistory> clipboard_history_;
  // Detects the search+v key combo.
  std::unique_ptr<ClipboardHistoryAcceleratorTarget> accelerator_target_;
  std::unique_ptr<ui::SimpleMenuModel::Delegate> menu_delegate_;
  // The items we show in the contextual menu. Saved so we can paste them later.
  std::vector<ui::ClipboardData> clipboard_items_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_H_