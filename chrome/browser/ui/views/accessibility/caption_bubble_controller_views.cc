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
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_)
    caption_widget_->CloseNow();
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;
}

void CaptionBubbleControllerViews::OnTranscription(
    const chrome::mojom::TranscriptionResultPtr& transcription_result) {
  if (!caption_bubble_)
    return;

  std::string partial_text = transcription_result->transcription;
  caption_bubble_->SetText(final_text_ + partial_text);
  if (transcription_result->is_final)
    final_text_ += partial_text;
  // TODO(1055150): Truncate final_text_ when it gets very long.
}

void CaptionBubbleControllerViews::OnActiveTabChanged(int index) {
  // Cache the index of the tab on which the caption bubble is meant to be
  // shown. When the caption bubble switches to that tab, show the widget. When
  // the caption bubble switches away from that tab, hide it.
  NOTIMPLEMENTED();
}

void CaptionBubbleControllerViews::UpdateCaptionStyle(
    base::Optional<ui::CaptionStyle> caption_style) {
  caption_bubble_->UpdateCaptionStyle(caption_style);
}

}  // namespace captions
