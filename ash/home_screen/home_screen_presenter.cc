// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_presenter.h"

#include <string>

#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"

namespace ash {
namespace {

// The y offset for home screen animation when overview mode toggles.
constexpr int kOverviewAnimationYOffset = 100;

// The duration in milliseconds for home screen animation when overview mode
// toggles.
constexpr base::TimeDelta kOverviewAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

void UpdateOverviewSettings(ui::AnimationMetricsReporter* reporter,
                            ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(kOverviewAnimationDuration);
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

  void Start(bool enter) {
    enter_ = enter;
  }

  void Report(int value) override {
    // Emit the correct histogram. Note that we have multiple macro instances
    // since each macro instance should be called with a runtime constant.
      if (enter_) {
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.StateTransition.AnimationSmoothness.EnterOverview", value);
      } else {
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.StateTransition.AnimationSmoothness.ExitOverview", value);
      }
  }

 private:
  bool enter_ = false;

  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationMetricsReporter);
};

HomeScreenPresenter::HomeScreenPresenter(HomeScreenController* controller)
    : controller_(controller),
      overview_animation_metrics_reporter_(
          std::make_unique<OverviewAnimationMetricsReporter>()) {
  DCHECK(controller);
}

HomeScreenPresenter::~HomeScreenPresenter() = default;

void HomeScreenPresenter::ScheduleOverviewModeAnimation(bool start,
                                                        bool animate) {
  // If animating, set the source parameters first.
  if (animate) {
    controller_->delegate()->NotifyHomeLauncherAnimationTransition(
        HomeScreenDelegate::AnimationTrigger::kOverviewMode,
        /*launcher_will_show=*/!start);
    controller_->delegate()->UpdateYPositionAndOpacityForHomeLauncher(
        start ? 0 : kOverviewAnimationYOffset, start ? 1.f : 0.f,
        base::NullCallback());

    overview_animation_metrics_reporter_->Start(start);
  }

  controller_->delegate()->UpdateYPositionAndOpacityForHomeLauncher(
      start ? kOverviewAnimationYOffset : 0, start ? 0.f : 1.f,
      animate ? base::BindRepeating(&UpdateOverviewSettings,
                                    overview_animation_metrics_reporter_.get())
              : base::NullCallback());
}

}  // namespace ash
