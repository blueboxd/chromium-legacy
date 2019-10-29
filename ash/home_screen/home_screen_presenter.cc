// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_presenter.h"

#include <string>

#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"

namespace ash {
namespace {

// The y offset for home screen animation when overview mode toggles using slide
// animation.
constexpr int kOverviewSlideAnimationYOffset = 100;

// The duration in milliseconds for home screen animation when overview mode
// toggles using fade animation.
constexpr base::TimeDelta kOverviewSlideAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

// The target scale to which (or from which) the home screen will animate when
// overview is being shown (or hidden) using fade transitions while home screen
// is shown.
constexpr float kOverviewFadeAnimationScale = 0.92f;

// The home screen animation duration for transitions that accompany overview
// fading transitions.
constexpr base::TimeDelta kOverviewFadeAnimationDuration =
    base::TimeDelta::FromMilliseconds(350);

base::TimeDelta GetAnimationDurationForTransition(
    HomeScreenPresenter::TransitionType transition) {
  switch (transition) {
    case HomeScreenPresenter::TransitionType::kSlideHomeIn:
    case HomeScreenPresenter::TransitionType::kSlideHomeOut:
      return kOverviewSlideAnimationDuration;
    case HomeScreenPresenter::TransitionType::kScaleHomeIn:
    case HomeScreenPresenter::TransitionType::kScaleHomeOut:
      return kOverviewFadeAnimationDuration;
  }
}

HomeScreenPresenter::TransitionType GetOppositeTransition(
    HomeScreenPresenter::TransitionType transition) {
  switch (transition) {
    case HomeScreenPresenter::TransitionType::kSlideHomeIn:
      return HomeScreenPresenter::TransitionType::kSlideHomeOut;
    case HomeScreenPresenter::TransitionType::kSlideHomeOut:
      return HomeScreenPresenter::TransitionType::kSlideHomeIn;
    case HomeScreenPresenter::TransitionType::kScaleHomeIn:
      return HomeScreenPresenter::TransitionType::kScaleHomeOut;
    case HomeScreenPresenter::TransitionType::kScaleHomeOut:
      return HomeScreenPresenter::TransitionType::kScaleHomeIn;
  }
}

void UpdateOverviewSettings(ui::AnimationMetricsReporter* reporter,
                            base::TimeDelta duration,
                            ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  DCHECK(reporter);
  settings->SetAnimationMetricsReporter(reporter);
}

}  // namespace

class HomeScreenPresenter::OverviewAnimationMetricsReporter
    : public ui::AnimationMetricsReporter {
 public:
  OverviewAnimationMetricsReporter() = default;
  ~OverviewAnimationMetricsReporter() override = default;

  void Start(TransitionType transition) { transition_ = transition; }

  void Report(int value) override {
    DCHECK(transition_.has_value());

    // Emit the correct histogram. Note that we have multiple macro instances
    // since each macro instance should be called with a runtime constant.
    switch (transition_.value()) {
      case TransitionType::kSlideHomeOut:
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.StateTransition.AnimationSmoothness.EnterOverview", value);
        break;
      case TransitionType::kSlideHomeIn:
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.StateTransition.AnimationSmoothness.ExitOverview", value);
        break;
      case TransitionType::kScaleHomeOut:
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.StateTransition.AnimationSmoothness.FadeInOverview", value);
        break;
      case TransitionType::kScaleHomeIn:
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.StateTransition.AnimationSmoothness.FadeOutOverview", value);
        break;
    }
  }

 private:
  base::Optional<TransitionType> transition_;

  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationMetricsReporter);
};

HomeScreenPresenter::HomeScreenPresenter(HomeScreenController* controller)
    : controller_(controller),
      overview_animation_metrics_reporter_(
          std::make_unique<OverviewAnimationMetricsReporter>()) {
  DCHECK(controller);
}

HomeScreenPresenter::~HomeScreenPresenter() = default;

void HomeScreenPresenter::ScheduleOverviewModeAnimation(
    TransitionType transition,
    bool animate) {
  const bool showing_home = transition == TransitionType::kSlideHomeIn ||
                            transition == TransitionType::kScaleHomeIn;
  // If animating, set the source parameters first.
  if (animate) {
    HomeScreenDelegate::AnimationTrigger trigger =
        (transition == TransitionType::kSlideHomeIn ||
         transition == TransitionType::kSlideHomeOut)
            ? HomeScreenDelegate::AnimationTrigger::kOverviewModeSlide
            : HomeScreenDelegate::AnimationTrigger::kOverviewModeFade;

    controller_->delegate()->NotifyHomeLauncherAnimationTransition(
        trigger, showing_home);

    // Force the home view into the expected initial state without animation
    SetFinalHomeTransformForTransition(GetOppositeTransition(transition),
                                       base::TimeDelta());

    overview_animation_metrics_reporter_->Start(transition);
  }

  // Hide all transient child windows in the app list (e.g. uninstall dialog)
  // before starting the overview mode transition, and restore them when
  // reshowing the app list.
  aura::Window* app_list_window =
      controller_->delegate()->GetHomeScreenWindow();
  if (app_list_window) {
    for (auto* child : wm::GetTransientChildren(app_list_window)) {
      if (showing_home)
        child->Show();
      else
        child->Hide();
    }
  }

  SetFinalHomeTransformForTransition(
      transition, animate ? GetAnimationDurationForTransition(transition)
                          : base::TimeDelta());
}

void HomeScreenPresenter::SetFinalHomeTransformForTransition(
    TransitionType transition,
    base::TimeDelta animation_duration) {
  HomeScreenDelegate::UpdateAnimationSettingsCallback
      animation_settings_updater =
          !animation_duration.is_zero()
              ? base::BindRepeating(&UpdateOverviewSettings,
                                    overview_animation_metrics_reporter_.get(),
                                    animation_duration)
              : base::NullCallback();

  switch (transition) {
    case TransitionType::kSlideHomeIn:
      controller_->delegate()->UpdateYPositionAndOpacityForHomeLauncher(
          0 /*y_position_in_screen*/, 1.0 /*opacity*/,
          animation_settings_updater);
      break;
    case TransitionType::kSlideHomeOut:
      controller_->delegate()->UpdateYPositionAndOpacityForHomeLauncher(
          kOverviewSlideAnimationYOffset /*y_position_in_screen*/,
          0.0 /*opacity*/, animation_settings_updater);
      break;
    case TransitionType::kScaleHomeIn:
      controller_->delegate()->UpdateScaleAndOpacityForHomeLauncher(
          1.0 /*scale*/, 1.0 /*opacity*/, animation_settings_updater);
      break;
    case TransitionType::kScaleHomeOut:
      controller_->delegate()->UpdateScaleAndOpacityForHomeLauncher(
          kOverviewFadeAnimationScale /*scale*/, 0.0 /*opacity*/,
          animation_settings_updater);
      break;
  }
}

}  // namespace ash
