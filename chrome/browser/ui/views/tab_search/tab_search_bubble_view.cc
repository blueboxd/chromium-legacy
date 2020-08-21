// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// The min / max size available to the TabSearchBubbleView.
// These are arbitrary sizes that match those set by ExtensionPopup.
// TODO(tluk): Determine the correct size constraints for the
// TabSearchBubbleView.
constexpr gfx::Size kMinSize(25, 25);
constexpr gfx::Size kMaxSize(800, 600);

class TabSearchWebView : public views::WebView {
 public:
  TabSearchWebView(content::BrowserContext* browser_context,
                   TabSearchBubbleView* parent)
      : WebView(browser_context), parent_(parent) {}

  ~TabSearchWebView() override {
    if (timer_.has_value()) {
      UmaHistogramMediumTimes("Tabs.TabSearch.WindowDisplayedDuration",
                              timer_->Elapsed());
    }
  }

  // views::WebView:
  void PreferredSizeChanged() override {
    WebView::PreferredSizeChanged();
    parent_->OnWebViewSizeChanged();
  }

  void OnWebContentsAttached() override { SetVisible(false); }

  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override {
    // Don't actually do anything with this information until we have been
    // shown. Size changes will not be honored by lower layers while we are
    // hidden.
    if (!GetVisible()) {
      pending_preferred_size_ = new_size;
      return;
    }
    WebView::ResizeDueToAutoResize(web_contents, new_size);
  }

  void DocumentOnLoadCompletedInMainFrame() override {
    GetWidget()->Show();
    GetWebContents()->Focus();

    // Track window open times from when the bubble is first shown.
    timer_ = base::ElapsedTimer();
  }

  void DidStopLoading() override {
    if (GetVisible())
      return;

    SetVisible(true);
    ResizeDueToAutoResize(web_contents(), pending_preferred_size_);
  }

 private:
  TabSearchBubbleView* parent_;

  // What we should set the preferred width to once TabSearch has loaded.
  gfx::Size pending_preferred_size_;

  // Time the Tab Search window has been open.
  base::Optional<base::ElapsedTimer> timer_;
};

}  // namespace

void TabSearchBubbleView::CreateTabSearchBubble(
    content::BrowserContext* browser_context,
    views::View* anchor_view) {
  auto delegate =
      std::make_unique<TabSearchBubbleView>(browser_context, anchor_view);
  BubbleDialogDelegateView::CreateBubble(delegate.release());
}

TabSearchBubbleView::TabSearchBubbleView(
    content::BrowserContext* browser_context,
    views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      web_view_(AddChildView(
          std::make_unique<TabSearchWebView>(browser_context, this))) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->EnableSizingFromWebContents(kMinSize, kMaxSize);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUITabSearchURL));
}

TabSearchBubbleView::~TabSearchBubbleView() = default;

gfx::Size TabSearchBubbleView::CalculatePreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size preferred_size = views::View::CalculatePreferredSize();
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(kMaxSize);
  return preferred_size;
}

void TabSearchBubbleView::OnWebViewSizeChanged() {
  SizeToContents();
}
