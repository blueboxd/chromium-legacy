// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/privacy_info_view.h"

#include <memory>
#include <utility>

#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int kRowMarginDip = 4;
constexpr int kVerticalPaddingDip = 9;
constexpr int kLeftPaddingDip = 14;
constexpr int kRightPaddingDip = 4;
constexpr int kCellSpacingDip = 18;
constexpr int kIconSizeDip = 20;

}  // namespace

PrivacyInfoView::PrivacyInfoView(const int info_string_id,
                                 const int link_string_id)
    : info_string_id_(info_string_id), link_string_id_(link_string_id) {
  InitLayout();
}

PrivacyInfoView::~PrivacyInfoView() = default;

gfx::Size PrivacyInfoView::CalculatePreferredSize() const {
  const int preferred_width = views::View::CalculatePreferredSize().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int PrivacyInfoView::GetHeightForWidth(int width) const {
  const int used_width = kRowMarginDip + kLeftPaddingDip + info_icon_->width() +
                         kCellSpacingDip +
                         /*|text_view_| is here*/
                         kCellSpacingDip + close_button_->width() +
                         kRightPaddingDip + kRowMarginDip;
  const int available_width = width - used_width;
  const int text_height = text_view_->GetHeightForWidth(available_width);
  return kRowMarginDip + /*border*/ 1 + kVerticalPaddingDip + text_height +
         kVerticalPaddingDip + /*border*/ 1 + kRowMarginDip;
}

void PrivacyInfoView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      // Prevents closing the AppListView when a click event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void PrivacyInfoView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      // Prevents closing the AppListView when a tap event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void PrivacyInfoView::InitLayout() {
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kVerticalPaddingDip, kLeftPaddingDip, kVerticalPaddingDip,
                  kRightPaddingDip),
      kCellSpacingDip));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/1,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_MEDIUM),
      gfx::Insets(kRowMarginDip, kRowMarginDip), gfx::kGoogleGrey300));

  // Info icon.
  InitInfoIcon();

  // Text.
  InitText();

  // Set flex so that text takes up the right amount of horizontal space between
  // the info icon and close button.
  layout_manager->SetFlexForView(text_view_, 1);

  // Close button.
  InitCloseButton();
}

void PrivacyInfoView::InitInfoIcon() {
  info_icon_ = AddChildView(std::make_unique<views::ImageView>());
  info_icon_->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  info_icon_->SetImage(gfx::CreateVectorIcon(views::kInfoIcon, kIconSizeDip,
                                             gfx::kGoogleBlue600));
}

void PrivacyInfoView::InitText() {
  const base::string16 link = l10n_util::GetStringUTF16(link_string_id_);
  size_t offset;
  const base::string16 text =
      l10n_util::GetStringFUTF16(info_string_id_, link, &offset);
  auto text_view = std::make_unique<views::StyledLabel>(text, this);
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = text_view->GetDefaultFontList().Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL);
  style.override_color = gfx::kGoogleGrey900;
  text_view->AddStyleRange(gfx::Range(0, offset), style);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.override_color = gfx::kGoogleBlue700;
  text_view->AddStyleRange(gfx::Range(offset, offset + link.length()),
                           link_style);
  text_view->SetAutoColorReadabilityEnabled(false);
  text_view_ = AddChildView(std::move(text_view));
}

void PrivacyInfoView::InitCloseButton() {
  auto close_button = std::make_unique<views::ImageButton>(this);
  close_button->SetImage(views::ImageButton::STATE_NORMAL,
                         gfx::CreateVectorIcon(views::kCloseIcon, kIconSizeDip,
                                               gfx::kGoogleGrey700));
  close_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  base::string16 close_button_label(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SetAccessibleName(close_button_label);
  close_button->SetTooltipText(close_button_label);
  close_button->SetFocusBehavior(FocusBehavior::ALWAYS);
  constexpr int kImageButtonSizeDip = 40;
  constexpr int kIconMarginDip = (kImageButtonSizeDip - kIconSizeDip) / 2;
  close_button->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kIconMarginDip)));
  close_button->SizeToPreferredSize();

  // Ink ripple.
  close_button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  constexpr SkColor kInkDropBaseColor = gfx::kGoogleGrey900;
  constexpr float kInkDropVisibleOpacity = 0.06f;
  constexpr float kInkDropHighlightOpacity = 0.08f;
  close_button->set_ink_drop_visible_opacity(kInkDropVisibleOpacity);
  close_button->set_ink_drop_highlight_opacity(kInkDropHighlightOpacity);
  close_button->set_ink_drop_base_color(kInkDropBaseColor);
  close_button->set_has_ink_drop_action_on_click(true);
  views::InstallCircleHighlightPathGenerator(close_button.get());
  close_button_ = AddChildView(std::move(close_button));
}

bool PrivacyInfoView::IsCloseButton(views::Button* button) const {
  return button == close_button_;
}

}  // namespace ash
