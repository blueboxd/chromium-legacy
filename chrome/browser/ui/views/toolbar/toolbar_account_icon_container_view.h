// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCOUNT_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCOUNT_ICON_CONTAINER_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class AvatarToolbarButton;
class Browser;
class PageActionIconContainerView;

// A container view for user-account-related PageActionIconViews and the profile
// avatar icon.
class ToolbarAccountIconContainerView : public ToolbarIconContainerView,
                                        public PageActionIconView::Delegate {
 public:
  explicit ToolbarAccountIconContainerView(Browser* browser);
  ~ToolbarAccountIconContainerView() override;

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

  // PageActionIconView::Delegate:
  SkColor GetPageActionInkDropColor() const override;
  content::WebContents* GetWebContentsForPageActionIconView() override;
  std::unique_ptr<views::Border> CreatePageActionIconBorder() const override;

  // views::View:
  void OnThemeChanged() override;
  const char* GetClassName() const override;

  PageActionIconContainerView* page_action_icon_container() {
    return page_action_icon_container_view_;
  }
  AvatarToolbarButton* avatar_button() { return avatar_; }

  static const char kToolbarAccountIconContainerViewClassName[];

 private:
  // ToolbarIconContainerView:
  const views::View::Views& GetChildren() const override;

  SkColor GetIconColor() const;

  PageActionIconContainerView* page_action_icon_container_view_ = nullptr;

  AvatarToolbarButton* const avatar_ = nullptr;

  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarAccountIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCOUNT_ICON_CONTAINER_VIEW_H_
