// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_transition_animator.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

HotseatTransitionAnimator::HotseatTransitionAnimator(ShelfWidget* shelf_widget)
    : shelf_widget_(shelf_widget) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

HotseatTransitionAnimator::~HotseatTransitionAnimator() {
  StopObservingImplicitAnimations();
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void HotseatTransitionAnimator::OnHotseatStateChanged(HotseatState old_state,
                                                      HotseatState new_state) {
  DoAnimation(old_state, new_state);
}

void HotseatTransitionAnimator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HotseatTransitionAnimator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HotseatTransitionAnimator::OnImplicitAnimationsCompleted() {
  std::move(animation_complete_callback_).Run();
}

void HotseatTransitionAnimator::OnTabletModeStarting() {
  tablet_mode_transitioning_ = true;
}

void HotseatTransitionAnimator::OnTabletModeStarted() {
  tablet_mode_transitioning_ = false;
}

void HotseatTransitionAnimator::OnTabletModeEnding() {
  tablet_mode_transitioning_ = true;
}

void HotseatTransitionAnimator::OnTabletModeEnded() {
  tablet_mode_transitioning_ = false;
}

void HotseatTransitionAnimator::DoAnimation(HotseatState old_state,
                                            HotseatState new_state) {
  if (!ShouldDoAnimation(old_state, new_state))
    return;

  StopObservingImplicitAnimations();

  const bool animating_to_shown_hotseat = new_state == HotseatState::kShown;

  gfx::Rect target_bounds = shelf_widget_->GetOpaqueBackground()->bounds();
  target_bounds.set_height(ShelfConfig::Get()->in_app_shelf_size());
  target_bounds.set_y(
      animating_to_shown_hotseat ? ShelfConfig::Get()->system_shelf_size() : 0);
  shelf_widget_->GetAnimatingBackground()->SetBounds(target_bounds);

  int starting_y;
  if (animating_to_shown_hotseat) {
    // This animation is triggered after bounds have been set in the shelf. When
    // transitioning to HotseatState::kShown, the shelf increases in size. To
    // prevent the background from jumping, adjust the y position to account for
    // the size increase.
    starting_y = ShelfConfig::Get()->system_shelf_size() -
                 ShelfConfig::Get()->in_app_shelf_size();
  } else {
    starting_y = ShelfConfig::Get()->shelf_size();
  }
  gfx::Transform transform;
  const int y_offset = starting_y - target_bounds.y();
  transform.Translate(0, y_offset);
  shelf_widget_->GetAnimatingBackground()->SetTransform(transform);

  {
    ui::ScopedLayerAnimationSettings shelf_bg_animation_setter(
        shelf_widget_->GetAnimatingBackground()->GetAnimator());
    shelf_bg_animation_setter.SetTransitionDuration(
        ShelfConfig::Get()->hotseat_background_animation_duration());
    shelf_bg_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
    shelf_bg_animation_setter.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_complete_callback_ = base::BindOnce(
        &HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded,
        weak_ptr_factory_.GetWeakPtr(), old_state, new_state);
    shelf_bg_animation_setter.AddObserver(this);

    shelf_widget_->GetAnimatingBackground()->SetTransform(gfx::Transform());
  }

  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationStarted(old_state, new_state);
}

bool HotseatTransitionAnimator::ShouldDoAnimation(HotseatState old_state,
                                                  HotseatState new_state) {
  // The first HotseatState change when going to tablet mode should not be
  // animated.
  if (tablet_mode_transitioning_)
    return false;

  return (new_state == HotseatState::kShown ||
          old_state == HotseatState::kShown) &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

void HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded(
    HotseatState old_state,
    HotseatState new_state) {
  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationEnded(old_state, new_state);
}

}  // namespace ash
