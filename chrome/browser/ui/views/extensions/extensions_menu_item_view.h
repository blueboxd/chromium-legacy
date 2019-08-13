// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

class Browser;
class ExtensionsMenuButton;
class ToolbarActionViewController;

namespace views {
class ButtonController;
}  // namespace views

class ExtensionsMenuItemView : public views::View {
 public:
  ExtensionsMenuItemView(
      Browser* browser,
      std::unique_ptr<ToolbarActionViewController> controller);
  ~ExtensionsMenuItemView() override;

  void UpdatePinButton();

  views::ButtonController* primary_action_button_controller_for_testing();

 private:
  ExtensionsMenuButton* extensions_menu_button_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
