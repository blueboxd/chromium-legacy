// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_

#include <memory>

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Button;
class ImageView;
}  // namespace views

class ExtensionsContainer;
class ExtensionsMenuItemView;

// This bubble view displays a list of user extensions.
// TODO(pbos): Once there's more functionality in here (getting to
// chrome://extensions, pinning, extension settings), update this comment.
class ExtensionsMenuView : public views::ButtonListener,
                           public views::BubbleDialogDelegateView,
                           public ToolbarActionsModel::Observer {
 public:
  static constexpr gfx::Size kExtensionsMenuIconSize = gfx::Size(28, 28);

  ExtensionsMenuView(views::View* anchor_view,
                     Browser* browser,
                     ExtensionsContainer* extensions_container);
  ~ExtensionsMenuView() override;

  static void ShowBubble(views::View* anchor_view,
                         Browser* browser,
                         ExtensionsContainer* extensions_container);
  static bool IsShowing();
  static void Hide();
  static ExtensionsMenuView* GetExtensionsMenuViewForTesting();
  static std::unique_ptr<views::ImageView> CreateFixedSizeIconView();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  int GetDialogButtons() const override;
  bool ShouldSnapFrameWidth() const override;
  // TODO(crbug.com/1003072): This override is copied from PasswordItemsView to
  // contrain the width. It would be nice to have a unified way of getting the
  // preferred size to not duplicate the code.
  gfx::Size CalculatePreferredSize() const override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& item,
                            int index) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionMoved(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionLoadFailed() override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarVisibleCountChanged() override;
  void OnToolbarHighlightModeChanged(bool is_highlighting) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  std::vector<ExtensionsMenuItemView*> extensions_menu_items_for_testing() {
    return extensions_menu_items_;
  }
  views::Button* manage_extensions_button_for_testing() {
    return manage_extensions_button_for_testing_;
  }

 private:
  void Repopulate();
  std::unique_ptr<views::View> CreateExtensionButtonsContainer();

  Browser* const browser_;
  ExtensionsContainer* const extensions_container_;
  ToolbarActionsModel* const model_;
  ScopedObserver<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observer_;
  std::vector<ExtensionsMenuItemView*> extensions_menu_items_;

  views::Button* manage_extensions_button_for_testing_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_H_
