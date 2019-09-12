// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "ui/views/controls/button/button.h"

namespace content {
class WebContents;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

// View used to display the cookie controls ui.
class CookieControlsBubbleView : public LocationBarBubbleDelegateView,
                                 public views::ButtonListener,
                                 public CookieControlsView {
 public:
  static void ShowBubble(views::View* anchor_view,
                         views::Button* highlighted_button,
                         content::WebContents* web_contents,
                         CookieControlsController* controller,
                         CookieControlsController::Status status);

  static CookieControlsBubbleView* GetCookieBubble();

  // CookieControlsView:
  void OnStatusChanged(CookieControlsController::Status status) override;
  void OnBlockedCookiesCountChanged(int blocked_cookies) override;

 private:
  CookieControlsBubbleView(views::View* anchor_view,
                           content::WebContents* web_contents,
                           CookieControlsController* cookie_contols);
  ~CookieControlsBubbleView() override;

  void UpdateUi(CookieControlsController::Status status,
                bool has_blocked_cookies);

  // LocationBarBubbleDelegateView:
  void CloseBubble() override;
  int GetDialogButtons() const override;
  void Init() override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Singleton instance of the cookie bubble. The cookie bubble can only be
  // shown on the active browser window, so there is no case in which it will be
  // shown twice at the same time.
  static CookieControlsBubbleView* cookie_bubble_;

  CookieControlsController* controller_ = nullptr;

  CookieControlsController::Status status_ =
      CookieControlsController::Status::kUninitialized;

  int blocked_cookies_ = -1;

  views::ImageView* header_image_ = nullptr;
  views::Button* turn_on_button_ = nullptr;
  views::Button* turn_off_button_ = nullptr;
  views::Label* title_ = nullptr;
  views::Label* sub_title_ = nullptr;
  views::View* spacer_ = nullptr;

  ScopedObserver<CookieControlsController, CookieControlsView> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieControlsBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_BUBBLE_VIEW_H_
