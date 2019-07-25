// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/close_desk_button.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

// The inkdrop opacity for the ripple effect.
// TODO(minch): Migrate to use kLightInkRippleOpacity in AshColorProvider.
constexpr float kInkDropOpacity = 0.08f;

// The highlight opacity for the ripple effect.
// TODO(minch): Migrate to use kLightInkRippleOpacity in AshColorProvider.
constexpr float kInkDropHighlightOpacity = 0.08f;

// The corner radius of the background of the close icon.
constexpr int kCornerRadius = CloseDeskButton::kCloseButtonSize / 2;

// The color of the close icon.
constexpr SkColor kIconColor = gfx::kGoogleGrey200;

// The background color for the close icon.
// TODO(minch): Migrate to use BaseLayerType::kTransparentWithBlur in dark mode
// in AshColorProvider.
constexpr SkColor kBackgroundColor = SkColorSetA(gfx::kGoogleGrey900, 0xBC);

}  // namespace

CloseDeskButton::CloseDeskButton(views::ButtonListener* listener)
    : ImageButton(listener) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kDesksCloseDeskButtonIcon, kIconColor));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          kBackgroundColor, kCornerRadius)));

  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_visible_opacity(kInkDropOpacity);
  SetFocusPainter(nullptr);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

CloseDeskButton::~CloseDeskButton() = default;

const char* CloseDeskButton::GetClassName() const {
  return "CloseDeskButton";
}

std::unique_ptr<views::InkDrop> CloseDeskButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnHover(true);
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> CloseDeskButton::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
CloseDeskButton::CreateInkDropHighlight() const {
  auto highlight = ImageButton::CreateInkDropHighlight();
  highlight->set_visible_opacity(kInkDropHighlightOpacity);
  return highlight;
}

SkColor CloseDeskButton::GetInkDropBaseColor() const {
  // TODO(minch): Migrate to use AshColorProvider::GetRippleAttributes().
  return color_utils::GetColorWithMaxContrast(kBackgroundColor);
}

std::unique_ptr<views::InkDropMask> CloseDeskButton::CreateInkDropMask() const {
  return std::make_unique<views::RoundRectInkDropMask>(size(), gfx::Insets(),
                                                       kCornerRadius);
}

bool CloseDeskButton::DoesIntersectRect(const views::View* target,
                                        const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = target->GetLocalBounds();
  // Only increase the hittest area for touch events (which have a non-empty
  // bounding box), not for mouse event.
  if (!views::UsePointBasedTargeting(rect)) {
    button_bounds.Inset(
        gfx::Insets(-kCloseButtonSize / 2, -kCloseButtonSize / 2));
  }
  return button_bounds.Intersects(rect);
}

bool CloseDeskButton::DoesIntersectScreenRect(
    const gfx::Rect& screen_rect) const {
  gfx::Point origin = screen_rect.origin();
  View::ConvertPointFromScreen(this, &origin);
  return DoesIntersectRect(this, gfx::Rect(origin, screen_rect.size()));
}

}  // namespace ash
