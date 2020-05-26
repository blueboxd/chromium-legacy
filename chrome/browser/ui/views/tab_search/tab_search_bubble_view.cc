// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The min / max size available to the TabSearchBubbleView.
// These are arbitrary sizes that match those set by ExtensionPopup.
// TODO(tluk): Determine the correct size constraints for the
// TabSearchBubbleView.
constexpr gfx::Size kMinSize(25, 25);
constexpr gfx::Size kMaxSize(800, 600);

class TabSearchWebView : public views::WebView {
 public:
  TabSearchWebView(Profile* profile, TabSearchBubbleView* parent)
      : WebView(profile), parent_(parent) {}

  ~TabSearchWebView() override = default;

  // WebView:
  void PreferredSizeChanged() override {
    View::PreferredSizeChanged();
    parent_->OnWebViewSizeChanged();
  }

 private:
  TabSearchBubbleView* parent_;
};

}  // namespace

void TabSearchBubbleView::CreateTabSearchBubble(Profile* profile,
                                                views::View* anchor_view) {
  auto delegate =
      base::WrapUnique(new TabSearchBubbleView(profile, anchor_view));
  BubbleDialogDelegateView::CreateBubble(delegate.release())->Show();
}

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

TabSearchBubbleView::TabSearchBubbleView(Profile* profile,
                                         views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      web_view_(
          AddChildView(std::make_unique<TabSearchWebView>(profile, this))) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->EnableSizingFromWebContents(kMinSize, kMaxSize);
  web_view_->LoadInitialURL(GURL("chrome://about"));

  // TODO(crbug.com/1010589) WebContents are initially assumed to be visible by
  // default unless explicitly hidden. The WebContents need to be set to hidden
  // so that the visibility state of the document in JavaScript is correctly
  // initially set to 'hidden', and the 'visibilitychange' events correctly get
  // fired.
  web_view_->GetWebContents()->WasHidden();
}
