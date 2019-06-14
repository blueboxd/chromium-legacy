// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_home_controller.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event_target.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace ash {

KioskNextHomeController::KioskNextHomeController() {
  display::Screen::GetScreen()->AddObserver(this);

  home_screen_container_ = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_HomeScreenContainer);
  home_screen_container_->AddObserver(this);
  if (!home_screen_container_->children().empty()) {
    DCHECK_EQ(1u, home_screen_container_->children().size());
    home_screen_window_ = home_screen_container_->children()[0];
  }
}

KioskNextHomeController::~KioskNextHomeController() {
  display::Screen::GetScreen()->RemoveObserver(this);

  if (home_screen_container_)
    home_screen_container_->RemoveObserver(this);
}

void KioskNextHomeController::ShowHomeScreenView() {
  // Nothing to do because the contents of the app window are always shown
  // on the primary display.
  // HomeScreenController will show/hide the root home screen window.
}

aura::Window* KioskNextHomeController::GetHomeScreenWindow() {
  return home_screen_window_;
}

void KioskNextHomeController::UpdateYPositionAndOpacityForHomeLauncher(
    int y_position_in_screen,
    float opacity,
    UpdateAnimationSettingsCallback callback) {
  aura::Window* window = GetHomeScreenWindow();
  if (!window)
    return;

  const gfx::Transform translation(1.f, 0.f, 0.f, 1.f, 0.f,
                                   static_cast<float>(y_position_in_screen));
  ui::Layer* layer = window->layer();
  layer->GetAnimator()->StopAnimating();
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (!callback.is_null()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        layer->GetAnimator());
    callback.Run(settings.get());
  }
  layer->SetOpacity(opacity);
  layer->SetTransform(translation);
}

void KioskNextHomeController::UpdateAfterHomeLauncherShown() {}

base::Optional<base::TimeDelta>
KioskNextHomeController::GetOptionalAnimationDuration() {
  return base::nullopt;
}

bool KioskNextHomeController::ShouldShowShelfOnHomeScreen() const {
  return false;
}

bool KioskNextHomeController::ShouldShowStatusAreaOnHomeScreen() const {
  return true;
}

void KioskNextHomeController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  aura::Window* window = GetHomeScreenWindow();
  if (!window)
    return;

  // Update the window to reflect the display bounds.
  const gfx::Rect bounds =
      screen_util::SnapBoundsToDisplayEdge(window->bounds(), window);
  window->SetBounds(bounds);
  const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
  wm::GetWindowState(window)->OnWMEvent(&event);
}

void KioskNextHomeController::OnWindowAdded(aura::Window* new_window) {
  DCHECK(!home_screen_window_);
  DCHECK_EQ(new_window->type(), aura::client::WindowType::WINDOW_TYPE_NORMAL);
  home_screen_window_ = new_window;

  Shell::Get()->screen_orientation_controller()->LockOrientationForWindow(
      home_screen_window_, OrientationLockType::kLandscape);
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
}

void KioskNextHomeController::OnWillRemoveWindow(aura::Window* window) {
  DCHECK_EQ(home_screen_window_, window);
  Shell::Get()->RemovePreTargetHandler(this);
  home_screen_window_ = nullptr;
}

void KioskNextHomeController::OnWindowDestroying(aura::Window* window) {
  if (window == home_screen_container_)
    home_screen_container_ = nullptr;
}

void KioskNextHomeController::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  int component = wm::GetNonClientComponent(target, event->location());

  aura::Window* new_target =
      ToplevelWindowEventHandler::GetTargetForClientAreaGesture(event, target);

  if (new_target)
    target = new_target;

  if (!(target == home_screen_window_ || started_handling_events_))
    return;

  if (component == HTCLIENT && event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    auto* overview_controller = Shell::Get()->overview_controller();
    if (!overview_controller->InOverviewSession())
      overview_controller->ToggleOverview();
    started_handling_events_ = true;
  }

  if (started_handling_events_) {
    event->SetHandled();
    event->StopPropagation();

    if (event->type() == ui::EventType::ET_GESTURE_SCROLL_END ||
        event->type() == ui::EventType::ET_GESTURE_END)
      started_handling_events_ = false;
  }
}

}  // namespace ash
