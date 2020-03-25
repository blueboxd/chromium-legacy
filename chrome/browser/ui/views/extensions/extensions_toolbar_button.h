// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/controls/button/menu_button_controller.h"

class Browser;
class ExtensionsToolbarContainer;


// Button in the toolbar that provides access to the corresponding extensions
// menu.
class ExtensionsToolbarButton : public ToolbarButton,
                                public views::ButtonListener,
                                public views::WidgetObserver {
 public:
  ExtensionsToolbarButton(Browser* browser,
                          ExtensionsToolbarContainer* extensions_container);
  ~ExtensionsToolbarButton() override;

  void UpdateIcon();

 private:
  // ToolbarButton:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  const char* GetClassName() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  int GetIconSize() const;

  // A lock to keep the button pressed when a popup is visible.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  Browser* const browser_;
  views::MenuButtonController* menu_button_controller_;
  ExtensionsToolbarContainer* const extensions_container_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
