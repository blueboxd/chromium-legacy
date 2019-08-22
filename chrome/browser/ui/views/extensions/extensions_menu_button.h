// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Button;
class ImageButton;
class MenuButton;
}  // namespace views

class ExtensionsMenuButton : public HoverButton,
                             public views::ButtonListener,
                             public ToolbarActionViewDelegateViews {
 public:
  ExtensionsMenuButton(Browser* browser,
                       std::unique_ptr<ToolbarActionViewController> controller);
  ~ExtensionsMenuButton() override;

  // Update pin button icon, color, tooltip, and visibility based on pinned
  // state.
  void UpdatePinButton();

  static const char kClassName[];

 private:
  // views::Button:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ButtonListener:
  const char* GetClassName() const override;
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // ToolbarActionViewDelegateViews:
  views::View* GetAsView() override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::Button* GetReferenceButtonForPopup() override;
  content::WebContents* GetCurrentWebContents() const override;
  void UpdateState() override;
  bool IsMenuRunning() const override;

  // Configures the secondary (right-hand-side) view of this HoverButton.
  void ConfigureSecondaryView();

  bool IsPinned();

  Browser* const browser_;

  // Responsible for executing the extension's actions.
  const std::unique_ptr<ToolbarActionViewController> controller_;
  ToolbarActionsModel* const model_;

  // TODO(pbos): There's complicated configuration code in place since menus
  // can't be triggered from ImageButtons. When MenuRunner::RunMenuAt accepts
  // views::Buttons, turn this into a views::ImageButton and use
  // image_button_factory.h methods to configure it.
  views::MenuButton* context_menu_button_ = nullptr;

  views::ImageButton* pin_button_ = nullptr;

  // This controller is responsible for showing the context menu for an
  // extension.
  ExtensionContextMenuController context_menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BUTTON_H_
