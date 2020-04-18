// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_

#include <memory>
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/content_settings/core/common/cookie_controls_status.h"

// View for the cookie control icon in the Omnibox.
class CookieControlsIconView : public PageActionIconView,
                               public CookieControlsView {
 public:
  CookieControlsIconView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  ~CookieControlsIconView() override;

  // CookieControlsUI:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,

                       int blocked_cookies) override;
  void OnBlockedCookiesCountChanged(int blocked_cookies) override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  void UpdateImpl() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

 protected:
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  const char* GetClassName() const override;

 private:
  bool HasAssociatedBubble() const;
  bool ShouldBeVisible() const;

  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;
  bool has_blocked_cookies_ = false;

  std::unique_ptr<CookieControlsController> controller_;
  ScopedObserver<CookieControlsController, CookieControlsView> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieControlsIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_ICON_VIEW_H_
