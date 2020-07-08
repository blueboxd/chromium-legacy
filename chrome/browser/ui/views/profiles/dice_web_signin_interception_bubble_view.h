// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include "base/callback.h"
#include "base/gtest_prod_util.h"

namespace content {
class BrowserContext;
}

namespace views {
class View;
}  // namespace views

// Bubble shown as part of Dice web signin interception. This bubble is
// implemented as a WebUI page rendered inside a native bubble.
class DiceWebSigninInterceptionBubbleView
    : public views::BubbleDialogDelegateView {
 public:
  ~DiceWebSigninInterceptionBubbleView() override;

  DiceWebSigninInterceptionBubbleView(
      const DiceWebSigninInterceptionBubbleView& other) = delete;
  DiceWebSigninInterceptionBubbleView& operator=(
      const DiceWebSigninInterceptionBubbleView& other) = delete;

  static void CreateBubble(content::BrowserContext* browser_context,
                           views::View* anchor_view,
                           base::OnceCallback<void(bool)> callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptionBubbleBrowserTest,
                           BubbleClosed);

  DiceWebSigninInterceptionBubbleView(content::BrowserContext* browser_context,
                                      views::View* anchor_view,
                                      base::OnceCallback<void(bool)> callback);

  // This bubble has no native buttons. The user accepts or cancels through this
  // method, which is called by the inner web UI.
  void OnWebUIUserChoice(bool accept);

  base::OnceCallback<void(bool)> callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_DICE_WEB_SIGNIN_INTERCEPTION_BUBBLE_VIEW_H_
