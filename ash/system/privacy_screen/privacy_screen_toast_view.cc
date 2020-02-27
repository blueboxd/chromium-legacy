// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_toast_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

// View containing the various labels in the toast.
class PrivacyScreenToastLabelView : public views::View {
 public:
  PrivacyScreenToastLabelView() {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    main_label_ = new views::Label();
    sub_label_ = new views::Label();
    AddChildView(main_label_);
    AddChildView(sub_label_);

    const AshColorProvider* color_provider = AshColorProvider::Get();
    const SkColor primary_text_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextPrimary,
        AshColorProvider::AshColorMode::kDark);
    const SkColor secondary_text_color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextSecondary,
        AshColorProvider::AshColorMode::kDark);

    ConfigureLabel(main_label_, primary_text_color,
                   kPrivacyScreenToastMainLabelFontSize);
    ConfigureLabel(sub_label_, secondary_text_color,
                   kPrivacyScreenToastSubLabelFontSize);

    main_label_->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_LABEL));
  }

  void SetPrivacyScreenEnabled(bool enabled) {
    if (enabled) {
      sub_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_SUBLABEL));
    } else {
      sub_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_SUBLABEL));
    }
  }

 private:
  void ConfigureLabel(views::Label* label, SkColor color, int font_size) {
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    label->SetEnabledColor(color);

    gfx::Font default_font;
    gfx::Font label_font =
        default_font.Derive(font_size - default_font.GetFontSize(),
                            gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
    gfx::FontList font_list(label_font);
    label->SetFontList(font_list);
  }

  views::Label* main_label_;
  views::Label* sub_label_;
};

PrivacyScreenToastView::PrivacyScreenToastView(
    views::ButtonListener* button_listener) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kPrivacyScreenToastInsets,
      kPrivacyScreenToastSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  button_ = new FeaturePodIconButton(button_listener, /*is_togglable=*/true);
  button_->SetVectorIcon(kPrivacyScreenIcon);
  button_->SetToggled(false);
  AddChildView(button_);

  label_ = new PrivacyScreenToastLabelView();
  AddChildView(label_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

PrivacyScreenToastView::~PrivacyScreenToastView() = default;

void PrivacyScreenToastView::SetPrivacyScreenEnabled(bool enabled) {
  button_->SetToggled(enabled);
  label_->SetPrivacyScreenEnabled(enabled);
  Layout();
}

gfx::Size PrivacyScreenToastView::CalculatePreferredSize() const {
  return gfx::Size(kPrivacyScreenToastWidth, kPrivacyScreenToastHeight);
}

}  // namespace ash
