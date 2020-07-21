// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARESHEET_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARESHEET_BUBBLE_VIEW_H_

#include <vector>

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace sharesheet {
class SharesheetServiceDelegate;
}

class SharesheetBubbleView : public views::BubbleDialogDelegateView {
 public:
  using TargetInfo = sharesheet::TargetInfo;

  SharesheetBubbleView(views::View* anchor_view,
                       sharesheet::SharesheetServiceDelegate* delegate);
  SharesheetBubbleView(const SharesheetBubbleView&) = delete;
  SharesheetBubbleView& operator=(const SharesheetBubbleView&) = delete;
  ~SharesheetBubbleView() override;

  void ShowBubble(std::vector<TargetInfo> targets);
  void CloseBubble();

  // views::BubbleDialogDelegateView overrides
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  // Owns this class.
  sharesheet::SharesheetServiceDelegate* delegate_;
  std::vector<TargetInfo> targets_;

  views::View* root_view_ = nullptr;
  views::View* main_view_ = nullptr;
  views::View* share_action_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARESHEET_BUBBLE_VIEW_H_
