// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

namespace captions {

// Static
std::unique_ptr<CaptionBubbleController> CaptionBubbleController::Create(
    Browser* browser) {
  return std::make_unique<CaptionBubbleControllerViews>(browser);
}

CaptionBubbleControllerViews::CaptionBubbleControllerViews(Browser* browser)
    : CaptionBubbleController(browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  caption_bubble_ = new CaptionBubble(
      browser_view->GetContentsView(),
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)));
  caption_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(caption_bubble_);

  // Temporary
  std::string text =
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter. She is known for narrative songs about her personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.";
  OnCaptionReceived(text);
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_)
    caption_widget_->CloseNow();
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;
}

void CaptionBubbleControllerViews::OnCaptionReceived(const std::string& text) {
  if (!caption_bubble_ || !caption_widget_)
    return;

  caption_bubble_->SetText(text);

  // If the captions bubble contains text, show the bubble. Otherwise, hide it.
  if (!caption_widget_->IsClosed()) {
    bool is_visible = caption_widget_->IsVisible();
    bool is_empty = text.empty();
    if (is_visible && is_empty) {
      caption_widget_->Hide();
    } else if (!is_visible && !is_empty) {
      caption_widget_->Show();
    }
  }
}

void CaptionBubbleControllerViews::OnActiveTabChanged(int index) {
  // Cache the index of the tab on which the caption bubble is meant to be
  // shown. When the caption bubble switches to that tab, show the widget. When
  // the caption bubble switches away from that tab, hide it.
  NOTIMPLEMENTED();
}

}  // namespace captions
