// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SK_ColorWHITE;
constexpr SkColor kFocusColor = SkColorSetA(gfx::kGoogleGrey900, 0x14);
constexpr SkColor kStrokeColor = SkColorSetA(gfx::kGoogleGrey900, 0x24);
constexpr SkColor kTextColor = gfx::kGoogleGrey700;
constexpr int kStrokeWidthDip = 1;
constexpr int kIconMarginDip = 8;
constexpr int kIconSizeDip = 16;
constexpr int kChipPaddingDip = 16;
constexpr int kPreferredHeightDip = 32;

}  // namespace

// SuggestionChipView ----------------------------------------------------------

// static
constexpr char SuggestionChipView::kClassName[];

SuggestionChipView::SuggestionChipView(AssistantViewDelegate* delegate,
                                       const AssistantSuggestion* suggestion,
                                       views::ButtonListener* listener)
    : Button(listener), delegate_(delegate), suggestion_(suggestion) {
  InitLayout();
}

SuggestionChipView::~SuggestionChipView() = default;

const char* SuggestionChipView::GetClassName() const {
  return kClassName;
}

gfx::Size SuggestionChipView::CalculatePreferredSize() const {
  const int preferred_width = views::View::CalculatePreferredSize().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int SuggestionChipView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void SuggestionChipView::ChildVisibilityChanged(views::View* child) {
  // When icon visibility is modified we need to update layout padding.
  if (child == icon_view_) {
    const int padding_left_dip =
        icon_view_->GetVisible() ? kIconMarginDip : kChipPaddingDip;
    layout_manager_->set_inside_border_insets(
        gfx::Insets(0, padding_left_dip, 0, kChipPaddingDip));
  }
  PreferredSizeChanged();
}

void SuggestionChipView::InitLayout() {
  const base::string16 text = base::UTF8ToUTF16(suggestion_->text);

  // Accessibility.
  SetAccessibleName(text);

  // Focus.
  // Note that we don't install the default focus ring as we use custom
  // highlighting instead.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(false);

  // Layout.
  // Note that padding differs depending on icon visibility.
  const int padding_left_dip =
      suggestion_->icon_url.is_empty() ? kChipPaddingDip : kIconMarginDip;

  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, padding_left_dip, 0, kChipPaddingDip), kIconMarginDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Icon.
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon_view_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));

  // Download our icon if necessary. Note that we *don't* hide the associated
  // view while an image is being downloaded. This prevents layout jank that
  // would otherwise occur when the image is finally rendered.
  if (suggestion_->icon_url.is_empty()) {
    icon_view_->SetVisible(false);
  } else {
    delegate_->DownloadImage(suggestion_->icon_url,
                             base::BindOnce(&SuggestionChipView::SetIcon,
                                            weak_factory_.GetWeakPtr()));
  }

  // Text.
  text_view_ = AddChildView(std::make_unique<views::Label>());
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetEnabledColor(kTextColor);
  text_view_->SetSubpixelRenderingEnabled(false);
  text_view_->SetFontList(
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(1));
  SetText(text);
}

void SuggestionChipView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  gfx::Rect bounds = GetContentsBounds();

  // Background.
  flags.setColor(kBackgroundColor);
  canvas->DrawRoundRect(bounds, height() / 2, flags);
  if (HasFocus()) {
    flags.setColor(kFocusColor);
    canvas->DrawRoundRect(bounds, height() / 2, flags);
  }

  // Border.
  // Stroke should be drawn within our contents bounds.
  bounds.Inset(gfx::Insets(kStrokeWidthDip));

  // Stroke.
  flags.setColor(kStrokeColor);
  flags.setStrokeWidth(kStrokeWidthDip);
  flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  canvas->DrawRoundRect(bounds, height() / 2, flags);
}

void SuggestionChipView::OnFocus() {
  SchedulePaint();
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void SuggestionChipView::OnBlur() {
  SchedulePaint();
}

bool SuggestionChipView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return false;
  return Button::OnKeyPressed(event);
}

void SuggestionChipView::SetIcon(const gfx::ImageSkia& icon) {
  icon_view_->SetImage(icon);
  icon_view_->SetVisible(!icon.isNull());
}

void SuggestionChipView::SetText(const base::string16& text) {
  text_view_->SetText(text);
}

const base::string16& SuggestionChipView::GetText() const {
  return text_view_->GetText();
}

}  // namespace ash
