// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button.h"

#include <math.h>  // std::ceil

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/logging.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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
const char HomeButton::kViewClassName[] = "ash/HomeButton";

HomeButton::HomeButton(Shelf* shelf)
    : ShelfControlButton(shelf, this), controller_(this) {
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  set_notify_action(Button::NOTIFY_ON_PRESS);
  set_has_ink_drop_action_on_click(false);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

HomeButton::~HomeButton() = default;

void HomeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (!controller_.MaybeHandleGestureEvent(event))
    Button::OnGestureEvent(event);
}

const char* HomeButton::GetClassName() const {
  return kViewClassName;
}

void HomeButton::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  DCHECK_EQ(button, this);
  if (!reverse && Shell::Get()->tablet_mode_controller() &&
      !Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    // We're trying to focus this button by advancing from the last view of
    // the shelf. Let the focus manager advance to the status area instead.
    shelf()->shelf_focus_cycler()->FocusOut(reverse, SourceView::kShelfView);
  }
}

void HomeButton::ButtonPressed(views::Button* sender,
                               const ui::Event& event,
                               views::InkDrop* ink_drop) {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON);
  const app_list::AppListShowSource show_source =
      event.IsShiftDown() ? app_list::kShelfButtonFullscreen
                          : app_list::kShelfButton;
  OnPressed(show_source, event.time_stamp());
}

void HomeButton::OnVoiceInteractionAvailabilityChanged() {
  SchedulePaint();
}

bool HomeButton::IsShowingAppList() const {
  return controller_.is_showing_app_list();
}

void HomeButton::OnPressed(app_list::AppListShowSource show_source,
                           base::TimeTicks time_stamp) {
  ShelfAction shelf_action =
      Shell::Get()->app_list_controller()->OnHomeButtonPressed(
          GetDisplayId(), show_source, time_stamp);
  if (shelf_action == SHELF_ACTION_APP_LIST_DISMISSED) {
    GetInkDrop()->SnapToActivated();
    GetInkDrop()->AnimateToState(views::InkDropState::HIDDEN);
  }
}

int64_t HomeButton::GetDisplayId() const {
  aura::Window* window = GetWidget()->GetNativeWindow();
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

void HomeButton::PaintButtonContents(gfx::Canvas* canvas) {
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

bool HomeButton::DoesIntersectRect(const views::View* target,
                                   const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = target->GetLocalBounds();
  // Increase clickable area for the button from
  // (kShelfControlSize x kShelfButtonSize) to
  // (kShelfButtonSize x kShelfButtonSize).
  int left_offset = button_bounds.width() - ShelfConstants::button_size();
  int bottom_offset = button_bounds.height() - ShelfConstants::button_size();
  button_bounds.Inset(gfx::Insets(0, left_offset, bottom_offset, 0));
  return button_bounds.Intersects(rect);
}

}  // namespace ash
