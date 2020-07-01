// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARESHEET_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARESHEET_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class SharesheetBubbleView : public views::BubbleDialogDelegateView {
 public:
  explicit SharesheetBubbleView(views::View* anchor_view);
  SharesheetBubbleView(const SharesheetBubbleView&) = delete;
  SharesheetBubbleView& operator=(const SharesheetBubbleView&) = delete;
  ~SharesheetBubbleView() override;

  void ShowBubble();

  // views::BubbleDialogDelegateView overrides
  gfx::Size CalculatePreferredSize() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARESHEET_BUBBLE_VIEW_H_
