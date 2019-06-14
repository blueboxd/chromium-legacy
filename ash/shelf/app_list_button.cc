// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include <math.h>  // std::ceil

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/kiosk_next/kiosk_next_shell_controller_impl.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"

namespace ash {
namespace {

constexpr uint8_t kVoiceInteractionRunningAlpha = 255;     // 100% alpha
constexpr uint8_t kVoiceInteractionNotRunningAlpha = 138;  // 54% alpha

}  // namespace

// static
const char AppListButton::kViewClassName[] = "ash/AppListButton";

AppListButton::AppListButton(ShelfView* shelf_view, Shelf* shelf)
    : ShelfControlButton(shelf_view), controller_(this, shelf) {
  DCHECK(shelf_view);
  DCHECK(shelf);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  set_notify_action(Button::NOTIFY_ON_PRESS);
  set_has_ink_drop_action_on_click(false);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

AppListButton::~AppListButton() = default;

void AppListButton::OnGestureEvent(ui::GestureEvent* event) {
  if (!controller_.MaybeHandleGestureEvent(event, shelf_view()))
    Button::OnGestureEvent(event);
}

const char* AppListButton::GetClassName() const {
  return kViewClassName;
}

void AppListButton::OnVoiceInteractionAvailabilityChanged() {
  SchedulePaint();
}

bool AppListButton::IsShowingAppList() const {
  return controller_.is_showing_app_list();
}

void AppListButton::OnPressed(app_list::AppListShowSource show_source,
                              base::TimeTicks time_stamp) {
  ShelfAction shelf_action =
      Shell::Get()->app_list_controller()->OnAppListButtonPressed(
          shelf_view()->GetDisplayId(), show_source, time_stamp);
  if (shelf_action == SHELF_ACTION_APP_LIST_DISMISSED) {
    GetInkDrop()->SnapToActivated();
    GetInkDrop()->AnimateToState(views::InkDropState::HIDDEN);
  }
}

void AppListButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::PointF circle_center(GetCenterPoint());

  // Paint a white ring as the foreground for the app list circle. The ceil/dsf
  // math assures that the ring draws sharply and is centered at all scale
  // factors.
  float ring_outer_radius_dp = 7.f;
  float ring_thickness_dp = 1.5f;
  if (controller_.IsVoiceInteractionAvailable()) {
    ring_outer_radius_dp = 8.f;
    ring_thickness_dp = 1.f;
  }
  {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    circle_center.Scale(dsf);
    cc::PaintFlags fg_flags;
    fg_flags.setAntiAlias(true);
    fg_flags.setStyle(cc::PaintFlags::kStroke_Style);
    fg_flags.setColor(kShelfIconColor);

    if (controller_.IsVoiceInteractionAvailable()) {
      // active: 100% alpha, inactive: 54% alpha
      fg_flags.setAlpha(controller_.IsVoiceInteractionRunning()
                            ? kVoiceInteractionRunningAlpha
                            : kVoiceInteractionNotRunningAlpha);
    }

    const float thickness = std::ceil(ring_thickness_dp * dsf);
    const float radius = std::ceil(ring_outer_radius_dp * dsf) - thickness / 2;
    fg_flags.setStrokeWidth(thickness);
    // Make sure the center of the circle lands on pixel centers.
    canvas->DrawCircle(circle_center, radius, fg_flags);

    if (controller_.IsVoiceInteractionAvailable()) {
      fg_flags.setAlpha(255);
      const float kCircleRadiusDp = 5.f;
      fg_flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawCircle(circle_center, std::ceil(kCircleRadiusDp * dsf),
                         fg_flags);
    }
  }
}

bool AppListButton::DoesIntersectRect(const views::View* target,
                                      const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = target->GetLocalBounds();
  // Increase clickable area for the button from
  // (kShelfControlSize x kShelfButtonSize) to
  // (kShelfButtonSize x kShelfButtonSize).
  int left_offset = button_bounds.width() - kShelfButtonSize;
  int bottom_offset = button_bounds.height() - kShelfButtonSize;
  button_bounds.Inset(gfx::Insets(0, left_offset, bottom_offset, 0));
  return button_bounds.Intersects(rect);
}

}  // namespace ash
