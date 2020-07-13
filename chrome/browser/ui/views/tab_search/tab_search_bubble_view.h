// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_

#include "base/scoped_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace views {
class Widget;
class WidgetObserver;
}  // namespace views

class Browser;

class TabSearchBubbleView : public views::BubbleDialogDelegateView,
                            public views::WidgetObserver {
 public:
  // TODO(tluk): Since the Bubble is shown asynchronously, we shouldn't call
  // this if the Widget is hidden and yet to be revealed.
  static void CreateTabSearchBubble(Browser* browser);

  TabSearchBubbleView(Browser* browser, views::View* anchor_view);
  ~TabSearchBubbleView() override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  void OnWebViewSizeChanged();

 private:
#if defined(USE_AURA)
  // TabSearchWindowObserverAura deals with issues in bubble deactivation on
  // Aura platforms. See comments in OnWindowActivated().
  // These issues are not present on Mac.
  class TabSearchWindowObserverAura;

  // |window_observer_| is a helper that hooks into the TabSearchBubbleView's
  // widget lifecycle events.
  std::unique_ptr<TabSearchWindowObserverAura> window_observer_;
#endif

  views::WebView* web_view_;

  ScopedObserver<views::Widget, views::WidgetObserver> observed_anchor_widget_{
      this};
  ScopedObserver<views::Widget, views::WidgetObserver> observed_bubble_widget_{
      this};

  DISALLOW_COPY_AND_ASSIGN(TabSearchBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
