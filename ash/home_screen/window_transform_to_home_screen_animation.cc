// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/window_transform_to_home_screen_animation.h"

#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform.h"

namespace ash {

namespace {

// The time to do window transform to scale up to its original position or
// scale down to homescreen animation.
constexpr base::TimeDelta kWindowScaleUpOrDownTime =
    base::TimeDelta::FromMilliseconds(350);

// The delay to do window opacity fade out when scaling down the dragged window.
constexpr base::TimeDelta kWindowFadeOutDelay =
    base::TimeDelta::FromMilliseconds(100);

// The window scale down factor if we head to home screen after drag ends.
constexpr float kWindowScaleDownFactor = 0.001f;

}  // namespace

WindowTransformToHomeScreenAnimation::WindowTransformToHomeScreenAnimation(
    aura::Window* window,
    base::Optional<BackdropWindowMode> original_backdrop_mode)
    : window_(window), original_backdrop_mode_(original_backdrop_mode) {
  window_observer_.Add(window);

  ui::ScopedLayerAnimationSettings settings(window_->layer()->GetAnimator());
  settings.SetTransitionDuration(kWindowScaleUpOrDownTime);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings.AddObserver(this);
  window_->layer()->GetAnimator()->SchedulePauseForProperties(
      kWindowFadeOutDelay, ui::LayerAnimationElement::OPACITY);
  window_->layer()->SetTransform(GetWindowTransformToHomeScreen());
  window_->layer()->SetOpacity(0.f);
}

WindowTransformToHomeScreenAnimation::~WindowTransformToHomeScreenAnimation() =
    default;

void WindowTransformToHomeScreenAnimation::OnImplicitAnimationsCompleted() {
  // Minimize the dragged window after transform animation is completed.
  ScopedAnimationDisabler disable(window_);
  window_->Hide();
  WindowState::Get(window_)->Minimize();

  // Reset its transform to identity transform and its original backdrop mode.
  window_->layer()->SetTransform(gfx::Transform());
  window_->layer()->SetOpacity(1.f);
  if (original_backdrop_mode_.has_value())
    window_->SetProperty(kBackdropWindowMode, *original_backdrop_mode_);

  delete this;
}

void WindowTransformToHomeScreenAnimation::OnWindowDestroying(
    aura::Window* window) {
  window_ = nullptr;
  delete this;
}

gfx::Transform
WindowTransformToHomeScreenAnimation::GetWindowTransformToHomeScreen() {
  gfx::Transform transform;
  HomeScreenDelegate* home_screen_delegate =
      Shell::Get()->home_screen_controller()->delegate();
  DCHECK(home_screen_delegate);
  const gfx::Rect window_bounds = window_->GetBoundsInScreen();

  // The origin of bounds returned by GetBoundsInScreen() is transformed using
  // the window's transform. The transform that should be applied to the window
  // is calculated relative to the window bounds with no transforms applied, and
  // thus need the un-transformed window origin.
  gfx::Point origin_without_transform = window_bounds.origin();
  window_->transform().TransformPointReverse(&origin_without_transform);

  const gfx::Rect app_list_item_bounds =
      home_screen_delegate->GetInitialAppListItemScreenBoundsForWindow(window_);

  if (!app_list_item_bounds.IsEmpty()) {
    transform.Translate(
        app_list_item_bounds.x() - origin_without_transform.x(),
        app_list_item_bounds.y() - origin_without_transform.y());
    transform.Scale(
        float(app_list_item_bounds.width()) / float(window_bounds.width()),
        float(app_list_item_bounds.height()) / float(window_bounds.height()));
  } else {
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            window_);
    transform.Translate(work_area.width() / 2 - origin_without_transform.x(),
                        work_area.height() / 2 - origin_without_transform.y());
    transform.Scale(kWindowScaleDownFactor, kWindowScaleDownFactor);
  }
  return transform;
}

}  // namespace ash
