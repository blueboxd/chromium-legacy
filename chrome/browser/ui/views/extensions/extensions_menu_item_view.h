// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class ToolbarActionViewController;

namespace views {
class MenuButton;
}  // namespace views

// ExtensionsMenuItemView is a single row inside the extensions menu for a
// particular extension. Includes information about the extension in addition to
// a button to pin the extension to the toolbar and a button for accessing the
// associated context menu.
class ExtensionsMenuItemView : public views::View,
                               public views::MenuButtonListener {
 public:
  static constexpr int kSecondaryIconSizeDp = 16;

  ExtensionsMenuItemView(
      Browser* browser,
      std::unique_ptr<ToolbarActionViewController> controller);
  ~ExtensionsMenuItemView() override;

  // views::MenuButtonListener:
  void OnMenuButtonClicked(views::Button* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  void UpdatePinButton();

  ExtensionsMenuButton* primary_action_button_for_testing();

 private:
  ExtensionsMenuButton* const primary_action_button_;

  std::unique_ptr<ToolbarActionViewController> controller_;

  views::MenuButton* context_menu_button_ = nullptr;

  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
