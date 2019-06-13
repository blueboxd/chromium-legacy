// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_PAGE_ACTION_ICON_CONTAINER_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class AvatarToolbarButton;
class Browser;
class ManagePasswordsIconViews;

namespace autofill {
class LocalCardMigrationIconView;
class SaveCardIconView;
}  // namespace autofill

// A container view for user-account-related PageActionIconViews and the profile
// avatar icon.
class ToolbarPageActionIconContainerView : public ToolbarIconContainerView,
                                           public PageActionIconContainer,
                                           public PageActionIconView::Delegate {
 public:
  explicit ToolbarPageActionIconContainerView(Browser* browser);
  ~ToolbarPageActionIconContainerView() override;

  PageActionIconView* GetIconView(PageActionIconType icon_type);

  // Activate the first visible but inactive PageActionIconView for
  // accessibility.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

  // PageActionIconContainer:
  void UpdatePageActionIcon(PageActionIconType icon_type) override;
  void ExecutePageActionIconForTesting(PageActionIconType icon_type) override;

  // PageActionIconView::Delegate:
  SkColor GetPageActionInkDropColor() const override;
  content::WebContents* GetWebContentsForPageActionIconView() override;

  // views::View:
  void OnThemeChanged() override;

  autofill::LocalCardMigrationIconView* local_card_migration_icon_view() const {
    return local_card_migration_icon_view_;
  }

  autofill::SaveCardIconView* save_card_icon_view() const {
    return save_card_icon_view_;
  }

  ManagePasswordsIconViews* manage_passwords_icon_views() const {
    return manage_passwords_icon_views_;
  }

  AvatarToolbarButton* avatar_button() { return avatar_; }

 private:
  bool FocusInactiveBubbleForIcon(PageActionIconView* icon_view);

  autofill::LocalCardMigrationIconView* local_card_migration_icon_view_ =
      nullptr;
  autofill::SaveCardIconView* save_card_icon_view_ = nullptr;
  ManagePasswordsIconViews* manage_passwords_icon_views_ = nullptr;
  AvatarToolbarButton* avatar_ = nullptr;

  std::vector<PageActionIconView*> page_action_icons_;

  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarPageActionIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
