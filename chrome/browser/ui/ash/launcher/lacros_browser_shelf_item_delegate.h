// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LACROS_BROWSER_SHELF_ITEM_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LACROS_BROWSER_SHELF_ITEM_DELEGATE_H_

#include "ash/public/cpp/shelf_item_delegate.h"

// Shelf item delegate for the lacros-chrome browser shortcut; only one such
// item should exist.
class LacrosBrowserShelfItemDelegate : public ash::ShelfItemDelegate {
 public:
  LacrosBrowserShelfItemDelegate();
  LacrosBrowserShelfItemDelegate(const LacrosBrowserShelfItemDelegate&) =
      delete;
  LacrosBrowserShelfItemDelegate& operator=(
      const LacrosBrowserShelfItemDelegate&) = delete;
  ~LacrosBrowserShelfItemDelegate() override;

  // ash::ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LACROS_BROWSER_SHELF_ITEM_DELEGATE_H_
