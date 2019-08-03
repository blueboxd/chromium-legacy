// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/new_desk_button.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
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

// The text and icon color when the new desk button is enabled/disabled. The
// disabled color is 38% opacity of the enabled color.
constexpr SkColor kTextAndIconColor = gfx::kGoogleGrey200;
constexpr SkColor kDisabledTextAndIconColor =
    SkColorSetA(kTextAndIconColor, 0x61);

// The background color when the new desk button is enabled/disabled. The
// disabled color is 38% opacity of the enabled color.
// TODO(minch): Migrate to use ControlsLayerType::kInactiveControlBackground in
// AshColorProvider.
constexpr SkColor kBackgroundColor = SkColorSetA(SK_ColorWHITE, 0x1A);
constexpr SkColor kDisabledBackgroundColor = SkColorSetA(SK_ColorWHITE, 0xA);

}  // namespace

NewDeskButton::NewDeskButton(views::ButtonListener* listener)
    : LabelButton(listener,
                  l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kDesksNewDeskButtonIcon, kTextAndIconColor));
  SetImage(views::Button::STATE_DISABLED,
           gfx::CreateVectorIcon(kDesksNewDeskButtonIcon,
                                 kDisabledTextAndIconColor));
  SetEnabledTextColors(kTextAndIconColor);
  SetTextColor(views::Button::STATE_DISABLED, kDisabledTextAndIconColor);
  SetImageLabelSpacing(kImageLabelSpacing);
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_visible_opacity(kInkDropVisibleOpacity);
  SetFocusPainter(nullptr);
  UpdateButtonState();
}

void NewDeskButton::UpdateButtonState() {
  const bool enabled = DesksController::Get()->CanCreateDesks();
  if (GetEnabled() == enabled)
    return;

  // Notify the overview highlight if we are about to be disabled.
  if (!enabled) {
    OverviewSession* overview_session =
        Shell::Get()->overview_controller()->overview_session();
    DCHECK(overview_session);
    overview_session->highlight_controller()->OnViewDestroyingOrDisabling(this);
  }
  SetEnabled(enabled);
  SetBackground(views::CreateRoundedRectBackground(
      enabled ? kBackgroundColor : kDisabledBackgroundColor, kCornerRadius));
}

void NewDeskButton::OnButtonPressed() {
  auto* controller = DesksController::Get();
  if (controller->CanCreateDesks()) {
    controller->NewDesk(DesksCreationRemovalSource::kButton);
    UpdateButtonState();
  }
}

const char* NewDeskButton::GetClassName() const {
  return "NewDeskButton";
}

std::unique_ptr<views::InkDrop> NewDeskButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
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

gfx::Rect NewDeskButton::GetHighlightBoundsInScreen() {
  return GetBoundsInScreen();
}

gfx::RoundedCornersF NewDeskButton::GetRoundedCornersRadii() const {
  return gfx::RoundedCornersF(kCornerRadius);
}

void NewDeskButton::MaybeActivateHighlightedView() {
  if (!GetEnabled())
    return;

  OnButtonPressed();
}

void NewDeskButton::MaybeCloseHighlightedView() {}

}  // namespace ash
