// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ui/base/hit_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"

// CaptionBubble implementation of BubbleFrameView.
class CaptionBubbleFrameView : public views::BubbleFrameView {
 public:
  CaptionBubbleFrameView()
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()) {}
  ~CaptionBubbleFrameView() override = default;

  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    int hit = views::BubbleFrameView::NonClientHitTest(point);

    // After BubbleFrameView::NonClientHitTest processes the bubble-specific
    // hits such as the close button and the rounded corners, it checks hits to
    // the bubble's client view. Any hits to ClientFrameView::NonClientHitTest
    // return HTCLIENT or HTNOWHERE. Override these to return HTCAPTION in
    // order to make the entire widget draggable.
    return (hit == HTCLIENT || hit == HTNOWHERE) ? HTCAPTION : hit;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptionBubbleFrameView);
};

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

  label_.SetMultiLine(true);
  label_.SetMaxLines(2);
  int max_width = GetAnchorView()->width() * 0.8;
  label_.SetMaximumWidth(max_width);
  label_.SetEnabledColor(SK_ColorWHITE);
  label_.SetBackgroundColor(SK_ColorTRANSPARENT);
  label_.SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_.SetLineHeight(18);

  std::vector<std::string> font_names = {"Arial", "Helvetica"};
  gfx::FontList* font_list = new gfx::FontList(
      font_names, gfx::Font::FontStyle::NORMAL, 14, gfx::Font::Weight::NORMAL);
  label_.SetFontList(*font_list);

  // Add some dummy text while this is in development.
  std::string text =
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter. She is known for narrative songs about her personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.";
  label_.SetText(base::ASCIIToUTF16(text));

  AddChildView(&label_);
}

bool CaptionBubble::ShouldShowCloseButton() const {
  return true;
}

views::NonClientFrameView* CaptionBubble::CreateNonClientFrameView(
    views::Widget* widget) {
  CaptionBubbleFrameView* frame = new CaptionBubbleFrameView();
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::NO_SHADOW,
      gfx::kPlaceholderColor);
  border->SetCornerRadius(2);
  frame->SetBubbleBorder(std::move(border));
  return frame;
}

void CaptionBubble::SetText(const std::string& text) {
  label_.SetText(base::ASCIIToUTF16(text));
}

}  // namespace captions
