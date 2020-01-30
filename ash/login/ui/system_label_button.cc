// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/system_label_button.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// The color of the system button's text and icon in alert mode.
constexpr SkColor kSystemButtonContentColorAlert =
    SkColorSetA(gfx::kGoogleGrey900, SK_AlphaOPAQUE);
// The background color of the system button in alert mode.
constexpr SkColor kSystemButtonBackgroundColorAlert =
    SkColorSetA(gfx::kGoogleRed300, SK_AlphaOPAQUE);

// The color of the system button's text in default mode.
constexpr SkColor kSystemButtonContentColorDefault =
    SkColorSetA(gfx::kGoogleGrey200, SK_AlphaOPAQUE);

// The color of the base color used for ink drop in default mode.
constexpr SkColor kInkDropBaseColorDefault = SK_ColorWHITE;

constexpr int kUserInfoBubbleWidth = 192;
constexpr int kUserInfoBubbleExternalPadding = 8;
constexpr int kSystemButtonHeight = 32;
constexpr int kSystemButtonIconSize = 20;
constexpr int kSystemButtonMarginTopBottomDp = 6;
constexpr int kSystemButtonMarginLeftRightDp = 16;
constexpr int kSystemButtonBorderRadius = 16;
constexpr int kSystemButtonImageLabelSpacing = 8;
constexpr int kSystemButtonMaxLabelWidthDp =
    kUserInfoBubbleWidth - 2 * kUserInfoBubbleExternalPadding -
    kSystemButtonIconSize - kSystemButtonImageLabelSpacing -
    2 * kSystemButtonBorderRadius;

}  // namespace

SkPath GetSystemButtonHighlightPath(const views::View* view) {
  gfx::Rect rect(view->GetLocalBounds());
  return SkPath().addRoundRect(gfx::RectToSkRect(rect),
                               kSystemButtonBorderRadius,
                               kSystemButtonBorderRadius);
}

class SystemButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  SystemButtonHighlightPathGenerator() = default;
  SystemButtonHighlightPathGenerator(
      const SystemButtonHighlightPathGenerator&) = delete;
  SystemButtonHighlightPathGenerator& operator=(
      const SystemButtonHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return GetSystemButtonHighlightPath(view);
  }
};

SystemLabelButton::SystemLabelButton(views::ButtonListener* listener,
                                     const base::string16& text,
                                     DisplayType display_type)
    : LabelButton(listener, text) {
  SetImageLabelSpacing(kSystemButtonImageLabelSpacing);
  label()->SetMultiLine(true);
  label()->SetMaximumWidth(kSystemButtonMaxLabelWidthDp);
  label()->SetFontList(
      gfx::FontList().DeriveWithWeight(gfx::Font::Weight::BOLD));
  SetMinSize(gfx::Size(0, kSystemButtonHeight));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  if (display_type == SystemLabelButton::DisplayType::ALERT_WITH_ICON) {
    SetImage(
        views::Button::STATE_NORMAL,
        CreateVectorIcon(kLockScreenAlertIcon, kSystemButtonContentColorAlert));
  }
  SetTextSubpixelRenderingEnabled(false);
  SkColor default_background = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kInactiveControlBackground,
      AshColorProvider::AshColorMode::kDark);
  bool is_alert =
      display_type == SystemLabelButton::DisplayType::ALERT_WITH_ICON ||
      display_type == SystemLabelButton::DisplayType::ALERT_NO_ICON;
  background_color_ =
      is_alert ? kSystemButtonBackgroundColorAlert : default_background;
  SkColor font_color = is_alert ? kSystemButtonContentColorAlert
                                : kSystemButtonContentColorDefault;
  SetTextColor(views::Button::STATE_NORMAL, font_color);
  SetTextColor(views::Button::STATE_HOVERED, font_color);
  SetTextColor(views::Button::STATE_PRESSED, font_color);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<SystemButtonHighlightPathGenerator>());

  SetInkDropMode(InkDropMode::ON);
  if (is_alert) {
    const AshColorProvider::RippleAttributes ripple_attributes =
        AshColorProvider::Get()->GetRippleAttributes(background_color_);
    set_ink_drop_base_color(ripple_attributes.base_color);
    set_ink_drop_visible_opacity(ripple_attributes.inkdrop_opacity);
    set_ink_drop_highlight_opacity(ripple_attributes.highlight_opacity);
  } else {
    // using RippleAttributes here doesn't give visually satisfying results
    // in default display mode
    set_ink_drop_base_color(kInkDropBaseColorDefault);
  }

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
}

void SystemLabelButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(background_color_);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(GetSystemButtonHighlightPath(this), flags);
}

gfx::Insets SystemLabelButton::GetInsets() const {
  return gfx::Insets(
      kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp,
      kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp);
}

}  // namespace ash
