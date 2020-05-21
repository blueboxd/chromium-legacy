// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>

#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/accessibility/caption_controller_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"

namespace captions {

// Static
std::unique_ptr<CaptionBubbleController> CaptionBubbleController::Create(
    Browser* browser) {
  return std::make_unique<CaptionBubbleControllerViews>(browser);
}

// Static
views::View* CaptionBubbleControllerViews::GetCaptionBubbleAccessiblePane(
    Browser* browser) {
  CaptionController* caption_controller =
      CaptionControllerFactory::GetForProfileIfExists(browser->profile());
  if (caption_controller) {
    CaptionBubbleControllerViews* bubble_controller =
        static_cast<CaptionBubbleControllerViews*>(
            caption_controller->GetCaptionBubbleControllerForBrowser(browser));
    if (bubble_controller)
      return bubble_controller->GetFocusableCaptionBubble();
  }
  return nullptr;
}

CaptionBubbleControllerViews::CaptionBubbleControllerViews(Browser* browser)
    : CaptionBubbleController(browser), browser_(browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  caption_bubble_ = new CaptionBubble(
      browser_view->GetContentsView(), browser_view,
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)));
  caption_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(caption_bubble_);
  browser_->tab_strip_model()->AddObserver(this);
  active_contents_ = browser_->tab_strip_model()->GetActiveWebContents();
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_)
    caption_widget_->CloseNow();
  if (browser_) {
    DCHECK(browser_->tab_strip_model());
    browser_->tab_strip_model()->RemoveObserver(this);
  }
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;

  // The caption bubble is destroyed when the browser is destroyed. So if the
  // caption bubble was destroyed, then browser_ must also be nullptr.
  browser_ = nullptr;
}

void CaptionBubbleControllerViews::OnTranscription(
    const chrome::mojom::TranscriptionResultPtr& transcription_result,
    content::WebContents* web_contents) {
  if (!caption_bubble_)
    return;

  std::string& partial_text = caption_texts_[web_contents].partial_text;
  std::string& final_text = caption_texts_[web_contents].final_text;
  partial_text = transcription_result->transcription;
  SetCaptionBubbleText();
  if (transcription_result->is_final) {
    // If the first character of partial text isn't a space, add a space before
    // appending it to final text.
    // TODO(crbug.com/1055150): This feature is launching for English first.
    // Make sure spacing is correct for all languages.
    final_text += partial_text;
    if (partial_text.size() > 0 &&
        partial_text.compare(partial_text.size() - 1, 1, " ") != 0) {
      final_text += " ";
    }
  }
  // TODO(1055150): Truncate final_text_ when it gets very long.
}

void CaptionBubbleControllerViews::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!caption_bubble_ || !caption_widget_)
    return;
  if (!selection.active_tab_changed())
    return;
  if (selection.selected_tabs_were_removed)
    caption_texts_.erase(selection.old_contents);

  active_contents_ = selection.new_contents;
  SetCaptionBubbleText();
}

void CaptionBubbleControllerViews::SetCaptionBubbleText() {
  std::string text;
  if (active_contents_ && caption_texts_.count(active_contents_))
    text = caption_texts_[active_contents_].full_text();
  caption_bubble_->SetText(text);
}

void CaptionBubbleControllerViews::UpdateCaptionStyle(
    base::Optional<ui::CaptionStyle> caption_style) {
  caption_bubble_->UpdateCaptionStyle(caption_style);
}

views::View* CaptionBubbleControllerViews::GetFocusableCaptionBubble() {
  if (caption_widget_ && caption_widget_->IsVisible())
    return caption_bubble_;
  return nullptr;
}

}  // namespace captions
