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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
// Formatting constants
static constexpr int kLineHeightDip = 18;
static constexpr int kMaxHeightDip = kLineHeightDip * 2;
static constexpr int kCornerRadiusDip = 8;
static constexpr double kPreferredAnchorWidthPercentage = 0.8;
// Dark grey at 80% opacity.
static constexpr SkColor kCaptionBubbleColor = SkColorSetARGB(204, 30, 30, 30);

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
}  // namespace

namespace captions {

CaptionBubble::CaptionBubble(views::View* anchor,
                             base::OnceClosure destroyed_callback)
    : BubbleDialogDelegateView(anchor,
                               views::BubbleBorder::FLOAT,
                               views::BubbleBorder::Shadow::NO_SHADOW),
      destroyed_callback_(std::move(destroyed_callback)) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_draggable(true);
}

CaptionBubble::~CaptionBubble() = default;

void CaptionBubble::Init() {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  layout->SetDefault(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width*/ true));

  set_color(kCaptionBubbleColor);
  set_close_on_deactivate(false);

  label_.SetMultiLine(true);
  int max_width = GetAnchorView()->width() * kPreferredAnchorWidthPercentage;
  label_.SetMaximumWidth(max_width);
  label_.SetEnabledColor(SK_ColorWHITE);
  label_.SetBackgroundColor(SK_ColorTRANSPARENT);
  label_.SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_.SetLineHeight(kLineHeightDip);

  std::vector<std::string> font_names = {"Arial", "Helvetica"};
  label_.SetFontList(gfx::FontList(font_names, gfx::Font::FontStyle::NORMAL, 14,
                                   gfx::Font::Weight::NORMAL));

  SetPreferredSize(gfx::Size(max_width, kMaxHeightDip));

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
  border->SetCornerRadius(kCornerRadiusDip);
  frame->SetBubbleBorder(std::move(border));
  return frame;
}

void CaptionBubble::SetText(const std::string& text) {
  label_.SetText(base::ASCIIToUTF16(text));
}

}  // namespace captions
