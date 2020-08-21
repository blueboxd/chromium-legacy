// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_

#include "base/scoped_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Widget;
class WebView;
}  // namespace views

namespace content {
class BrowserContext;
}  // namespace content

class TabSearchBubbleView : public views::BubbleDialogDelegateView {
 public:
  // TODO(tluk): Since the Bubble is shown asynchronously, we shouldn't call
  // this if the Widget is hidden and yet to be revealed.
  static views::Widget* CreateTabSearchBubble(
      content::BrowserContext* browser_context,
      views::View* anchor_view);

  TabSearchBubbleView(content::BrowserContext* browser_context,
                      views::View* anchor_view);
  ~TabSearchBubbleView() override = default;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;

  void OnWebViewSizeChanged();

 private:
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(TabSearchBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
