// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_

#include <string>

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Label;
}

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble
//
//  A caption bubble that floats above the BrowserView and shows automatically-
//  generated text captions for audio and media streams from the current tab.
//
class CaptionBubble : public views::BubbleDialogDelegateView {
 public:
  explicit CaptionBubble(views::View* anchor,
                         base::OnceClosure destroyed_callback);
  ~CaptionBubble() override;
  CaptionBubble(const CaptionBubble&) = delete;
  CaptionBubble& operator=(const CaptionBubble&) = delete;

  // Set the text of the caption bubble. The bubble displays the last 2 lines.
  void SetText(const std::string& text);

 protected:
  void Init() override;
  bool ShouldShowCloseButton() const override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

 private:
  friend class CaptionBubbleControllerViewsTest;

  // Unowned. Owned by views hierarchy.
  views::Label* label_;
  views::Label* title_;

  base::ScopedClosureRunner destroyed_callback_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
