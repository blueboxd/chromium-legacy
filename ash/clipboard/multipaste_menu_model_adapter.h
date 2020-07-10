// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_MULTIPASTE_MENU_MODEL_ADAPTER_H_
#define ASH_CLIPBOARD_MULTIPASTE_MENU_MODEL_ADAPTER_H_

#include <memory>

#include "ui/views/controls/menu/menu_model_adapter.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class MenuItemView;
class MenuRunner;
}  // namespace views

namespace ash {

// Used to show the multipaste menu, which holds the last few things copied.
class MultipasteMenuModelAdapter : views::MenuModelAdapter {
 public:
  explicit MultipasteMenuModelAdapter(
      std::unique_ptr<ui::SimpleMenuModel> model);
  MultipasteMenuModelAdapter(const MultipasteMenuModelAdapter&) = delete;
  MultipasteMenuModelAdapter& operator=(const MultipasteMenuModelAdapter&) =
      delete;
  ~MultipasteMenuModelAdapter() override;

  // Shows the menu, anchored below |anchor_rect|.
  void Run(const gfx::Rect& anchor_rect);

 private:
  // The model which holds the contents of the menu.
  std::unique_ptr<ui::SimpleMenuModel> const model_;
  // The root MenuItemView which contains all child MenuItemViews. Owned by
  // |menu_runner_|.
  views::MenuItemView* root_view_ = nullptr;
  // Responsible for showing |root_view_|.
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_MULTIPASTE_MENU_MODEL_ADAPTER_H_