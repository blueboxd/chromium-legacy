// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
// Formatting constants
static constexpr int kLineHeightDip = 24;
static constexpr int kMaxHeightDip = kLineHeightDip * 2;
static constexpr int kCornerRadiusDip = 8;
static constexpr int kMaxWidthDip = 548;
static constexpr int kHorizontalMarginsDip = 6;
static constexpr int kVerticalMarginsDip = 8;
static constexpr double kPreferredAnchorWidthPercentage = 0.8;
// 90% opacity.
static constexpr int kCaptionBubbleAlpha = 230;
static constexpr char kPrimaryFont[] = "Roboto";
static constexpr char kSecondaryFont[] = "Arial";
static constexpr char kTertiaryFont[] = "sans-serif";
static constexpr int kFontSizePx = 16;

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

  // TODO(crbug.com/1055150): Use system caption color scheme rather than
  // hard-coding the colors.
  SkColor caption_bubble_color_ =
      SkColorSetA(gfx::kGoogleGrey900, kCaptionBubbleAlpha);
  set_color(caption_bubble_color_);
  set_close_on_deactivate(false);

  auto label = std::make_unique<views::Label>();
  label->SetMultiLine(true);
  label->SetMaximumWidth(kMaxWidthDip);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetLineHeight(kLineHeightDip);
  label->SetTooltipText(base::string16());

  // TODO(crbug.com/1055150): Respect the user's font size and minimum font size
  // settings rather than having a fixed font size.
  const gfx::FontList font_list = gfx::FontList(
      {kPrimaryFont, kSecondaryFont, kTertiaryFont},
      gfx::Font::FontStyle::NORMAL, kFontSizePx, gfx::Font::Weight::NORMAL);
  label->SetFontList(font_list);

  auto title = std::make_unique<views::Label>();
  title->SetEnabledColor(gfx::kGoogleGrey500);
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  title->SetLineHeight(kLineHeightDip);
  title->SetFontList(font_list);
  title->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));

  // TODO(crbug.com/1055150): Resize responsively with anchor size changes.
  int min_width = GetAnchorView()->width() * kPreferredAnchorWidthPercentage;
  int width = std::min(min_width, kMaxWidthDip);
  SetPreferredSize(gfx::Size(width, kMaxHeightDip));
  set_margins(gfx::Insets(kHorizontalMarginsDip, kVerticalMarginsDip));

  title_ = AddChildView(std::move(title));
  label_ = AddChildView(std::move(label));
}

bool CaptionBubble::ShouldShowCloseButton() const {
  return true;
}

views::NonClientFrameView* CaptionBubble::CreateNonClientFrameView(
    views::Widget* widget) {
  CaptionBubbleFrameView* frame = new CaptionBubbleFrameView();
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW,
      gfx::kPlaceholderColor);
  border->SetCornerRadius(kCornerRadiusDip);
  frame->SetBubbleBorder(std::move(border));
  return frame;
}

void CaptionBubble::SetText(const std::string& text) {
  label_->SetText(base::ASCIIToUTF16(text));
  // Show the title if there is room for it.
  title_->SetVisible(label_->GetPreferredSize().height() < kMaxHeightDip);
}

}  // namespace captions
