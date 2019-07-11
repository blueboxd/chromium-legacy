// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/new_desk_button.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 16;

constexpr int kImageLabelSpacing = 8;

constexpr float kInkDropVisibleOpacity = 0.2f;

constexpr float kInkDropHighlightVisibleOpacity = 0.3f;

constexpr SkColor kHighlightBackgroundColor = SkColorSetARGB(60, 255, 255, 255);

}  // namespace

NewDeskButton::NewDeskButton(views::ButtonListener* listener)
    : LabelButton(listener,
                  l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kDesksNewDeskButtonIcon, SK_ColorWHITE));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(kDesksNewDeskButtonIcon, SK_ColorGRAY));
  SetTextColor(views::Button::STATE_NORMAL, SK_ColorWHITE);
  SetTextColor(views::Button::STATE_HOVERED, SK_ColorWHITE);
  SetTextColor(views::Button::STATE_PRESSED, SK_ColorWHITE);
  SetTextColor(views::Button::STATE_DISABLED, SK_ColorGRAY);
  SetImageLabelSpacing(kImageLabelSpacing);
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_visible_opacity(kInkDropVisibleOpacity);
  SetFocusPainter(nullptr);
  SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          kHighlightBackgroundColor, kCornerRadius)));
}

const char* NewDeskButton::GetClassName() const {
  return "NewDeskButton";
}

std::unique_ptr<views::InkDrop> NewDeskButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnHover(true);
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropHighlight> NewDeskButton::CreateInkDropHighlight()
    const {
  auto highlight = LabelButton::CreateInkDropHighlight();
  highlight->set_visible_opacity(kInkDropHighlightVisibleOpacity);
  return highlight;
}

SkColor NewDeskButton::GetInkDropBaseColor() const {
  return SK_ColorWHITE;
}

std::unique_ptr<views::InkDropMask> NewDeskButton::CreateInkDropMask() const {
  return std::make_unique<views::RoundRectInkDropMask>(size(), gfx::Insets(),
                                                       kCornerRadius);
}

std::unique_ptr<views::LabelButtonBorder> NewDeskButton::CreateDefaultBorder()
    const {
  std::unique_ptr<views::LabelButtonBorder> border =
      std::make_unique<views::LabelButtonBorder>();
  return border;
}

views::View* NewDeskButton::GetView() {
  return this;
}

gfx::Rect NewDeskButton::GetHighlightBounds() {
  return GetBoundsInScreen();
}

}  // namespace ash
