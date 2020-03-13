// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_bubble.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace captions {

CaptionBubble::CaptionBubble(views::View* anchor)
    : BubbleDialogDelegateView(anchor,
                               views::BubbleBorder::FLOAT,
                               views::BubbleBorder::Shadow::NO_SHADOW) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_draggable(true);
}

CaptionBubble::~CaptionBubble() = default;

// static
void CaptionBubble::CreateAndShow(views::View* anchor) {
  CaptionBubble* caption_bubble = new CaptionBubble(anchor);
  views::BubbleDialogDelegateView::CreateBubble(caption_bubble);
  caption_bubble->GetWidget()->Show();
}

void CaptionBubble::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(10)));
  set_color(SK_ColorGRAY);
  set_close_on_deactivate(false);

  label_ = new views::Label();
  label_->SetMultiLine(true);
  label_->SetMaxLines(2);
  label_->SetElideBehavior(gfx::TRUNCATE_HEAD);
  int max_width = GetAnchorView()->width() * 0.8;
  label_->SetMaximumWidth(max_width);
  label_->SetEnabledColor(SK_ColorWHITE);
  label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetLineHeight(18);

  std::vector<std::string> font_names = {"Arial", "Helvetica"};
  gfx::FontList* font_list = new gfx::FontList(
      font_names, gfx::Font::FontStyle::NORMAL, 14, gfx::Font::Weight::NORMAL);
  label_->SetFontList(*font_list);

  // Add some dummy text while this is in development.
  std::string text =
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter. She is known for narrative songs about her personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.";
  label_->SetText(base::ASCIIToUTF16(text));

  AddChildView(label_);
}

bool CaptionBubble::ShouldShowCloseButton() const {
  return true;
}

void CaptionBubble::SetText(const std::string& text) {
  label_->SetText(base::ASCIIToUTF16(text));
}

}  // namespace captions
