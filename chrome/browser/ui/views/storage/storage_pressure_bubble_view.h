// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "url/origin.h"

class Browser;

class StoragePressureBubbleView : public views::BubbleDialogDelegateView {
 public:
  static void ShowBubble(const url::Origin origin);

 private:
  StoragePressureBubbleView(views::View* anchor_view,
                            const gfx::Rect& anchor_rect,
                            Browser* browser,
                            const url::Origin origin);

  void OnDialogAccepted();

  // views::DialogDelegate:
  base::string16 GetWindowTitle() const override;

  // views::BubbleDialogDelegateView:
  void Init() override;

  Browser* const browser_;
  const GURL all_sites_url_ = GURL("chrome://settings/content/all");
  const url::Origin origin_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_
