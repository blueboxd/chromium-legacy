// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

// TODO(tluk): Only show the bubble once web contents are available to prevent
// akward resizing when web content finally loads in.
class TabSearchBubbleView : public views::BubbleDialogDelegateView {
 public:
  static void CreateTabSearchBubble(Profile* profile, views::View* anchor_view);

  ~TabSearchBubbleView() override = default;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  void OnWebViewSizeChanged();

 private:
  TabSearchBubbleView(Profile* profile, views::View* anchor_view);

  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(TabSearchBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
