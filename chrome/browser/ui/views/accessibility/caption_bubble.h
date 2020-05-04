// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_

#include <string>

#include "ui/native_theme/caption_style.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
class ImageButton;
class ImageView;
}

namespace ui {
struct AXNodeData;
}

namespace captions {
class CaptionBubbleFrameView;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble
//
//  A caption bubble that floats above the BrowserView and shows automatically-
//  generated text captions for audio and media streams from the current tab.
//
class CaptionBubble : public views::BubbleDialogDelegateView,
                      public views::ButtonListener {
 public:
  CaptionBubble(views::View* anchor, base::OnceClosure destroyed_callback);
  ~CaptionBubble() override;
  CaptionBubble(const CaptionBubble&) = delete;
  CaptionBubble& operator=(const CaptionBubble&) = delete;

  // Set the text of the caption bubble. The bubble displays the last 2 lines.
  void SetText(const std::string& text);

  // Displays an error if |has_error|, otherwise displays the latest text.
  void SetHasError(bool has_error);

  // Changes the caption style of the caption bubble. For now, this only sets
  // the caption text size.
  void UpdateCaptionStyle(base::Optional<ui::CaptionStyle> caption_style);

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  bool ShouldShowCloseButton() const override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  gfx::Rect GetBubbleBounds() override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnFocus() override;
  void OnBlur() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  friend class CaptionBubbleControllerViewsTest;

  void UpdateTitleVisibility();
  double GetTextScaleFactor();
  void UpdateTextSize();

  // Unowned. Owned by views hierarchy.
  views::Label* label_;
  views::Label* title_;
  views::Label* error_message_;
  views::ImageView* error_icon_;
  views::ImageButton* close_button_;
  CaptionBubbleFrameView* frame_;
  views::View* content_container_;

  bool has_error_ = false;

  base::Optional<ui::CaptionStyle> caption_style_;

  base::ScopedClosureRunner destroyed_callback_;

  // The bubble tries to stay relatively positioned in its parent.
  // ratio_in_parent_x_ represents the ratio along the parent width at which
  // to display the center of the bubble, if possible.
  double ratio_in_parent_x_;
  double ratio_in_parent_y_;
  gfx::Rect latest_bounds_;
  gfx::Rect latest_anchor_bounds_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
