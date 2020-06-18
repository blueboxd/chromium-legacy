// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_model.h"

#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {
// The caption bubble contains 2 lines of text in its normal size and 4 lines
// in its expanded size, so the maximum number of lines before truncating is 5.
constexpr int kMaxLines = 5;
}  // namespace

namespace captions {

CaptionBubbleModel::CaptionBubbleModel(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

CaptionBubbleModel::~CaptionBubbleModel() {
  if (observer_)
    observer_->SetModel(nullptr);
}

void CaptionBubbleModel::SetObserver(CaptionBubble* observer) {
  if (observer_)
    return;
  observer_ = observer;
  if (observer_) {
    observer_->OnTextChange();
    observer_->OnErrorChange();
  }
}

void CaptionBubbleModel::RemoveObserver() {
  observer_ = nullptr;
}

void CaptionBubbleModel::OnTextChange() {
  if (observer_)
    observer_->OnTextChange();
}

void CaptionBubbleModel::SetPartialText(const std::string& partial_text) {
  partial_text_ = partial_text;
  OnTextChange();
}

void CaptionBubbleModel::Close() {
  final_text_.clear();
  partial_text_.clear();
  is_closed_ = true;
  OnTextChange();
}

void CaptionBubbleModel::SetHasError(bool has_error) {
  has_error_ = has_error;
  if (observer_)
    observer_->OnErrorChange();
}

void CaptionBubbleModel::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  // Reset caption bubble to it's starting state.
  final_text_.clear();
  partial_text_.clear();
  is_closed_ = false;
  has_error_ = false;
  OnTextChange();
}

void CaptionBubbleModel::CommitPartialText() {
  final_text_ += partial_text_;

  // If the first character of partial text isn't a space, add a space before
  // appending it to final text. There is no need to alert the observer because
  // the text itself has not changed, just its representation, and there is no
  // need to render a trailing space.
  // TODO(crbug.com/1055150): This feature is launching for English first.
  // Make sure spacing is correct for all languages.
  if (partial_text_.size() > 0 &&
      partial_text_.compare(partial_text_.size() - 1, 1, " ") != 0) {
    final_text_ += " ";
  }
  partial_text_.clear();

  if (!observer_)
    return;

  // Truncate the final text to kMaxLines lines long. This time, alert the
  // observer that the text has changed.
  const size_t num_lines = observer_->GetNumLinesInLabel();
  if (num_lines > kMaxLines) {
    DCHECK(base::IsStringASCII(final_text_));
    const size_t truncate_index =
        observer_->GetTextIndexOfLineInLabel(num_lines - kMaxLines);
    final_text_.erase(0, truncate_index);
    OnTextChange();
  }
}

}  // namespace captions
