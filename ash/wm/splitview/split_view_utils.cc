// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_utils.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The animation speed at which the highlights fade in or out.
constexpr base::TimeDelta kHighlightsFadeInOut =
    base::TimeDelta::FromMilliseconds(250);
// The animation speed which the other highlight fades in or out.
constexpr base::TimeDelta kOtherFadeInOut =
    base::TimeDelta::FromMilliseconds(133);
// The delay before the other highlight starts fading in.
constexpr base::TimeDelta kOtherFadeInDelay =
    base::TimeDelta::FromMilliseconds(117);
// The animation speed at which the preview area fades out (when you snap a
// window).
constexpr base::TimeDelta kPreviewAreaFadeOut =
    base::TimeDelta::FromMilliseconds(67);
// The time duration for the indicator label opacity animations.
constexpr base::TimeDelta kLabelAnimation =
    base::TimeDelta::FromMilliseconds(83);
// The delay before the indicator labels start fading in.
constexpr base::TimeDelta kLabelAnimationDelay =
    base::TimeDelta::FromMilliseconds(167);
// The time duration for the window transformation animations.
constexpr base::TimeDelta kWindowTransform =
    base::TimeDelta::FromMilliseconds(kSplitviewWindowTransformMs);

constexpr float kHighlightOpacity = 0.3f;
constexpr float kPreviewAreaHighlightOpacity = 0.18f;

// Toast data.
constexpr char kAppCannotSnapToastId[] = "split_view_app_cannot_snap";
constexpr int kAppCannotSnapToastDurationMs = 2500;

// Gets the duration, tween type and delay before animation based on |type|.
void GetAnimationValuesForType(
    SplitviewAnimationType type,
    base::TimeDelta* out_duration,
    gfx::Tween::Type* out_tween_type,
    ui::LayerAnimator::PreemptionStrategy* out_preemption_strategy,
    base::TimeDelta* out_delay) {
  *out_preemption_strategy = ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET;
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT:
      *out_duration = kHighlightsFadeInOut;
      *out_tween_type = gfx::Tween::FAST_OUT_SLOW_IN;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN:
      *out_delay = kOtherFadeInDelay;
      *out_duration = kOtherFadeInOut;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      *out_preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT:
      *out_duration = kOtherFadeInOut;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
      *out_duration = kPreviewAreaFadeOut;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
      *out_delay = kLabelAnimationDelay;
      *out_duration = kLabelAnimation;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      *out_preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
      return;
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
      *out_duration = kLabelAnimation;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM:
      *out_duration = kWindowTransform;
      *out_tween_type = gfx::Tween::FAST_OUT_SLOW_IN;
      *out_preemption_strategy =
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;
      return;
  }

  NOTREACHED();
}

// Helper function to apply animation values to |settings|.
void ApplyAnimationSettings(
    ui::ScopedLayerAnimationSettings* settings,
    ui::LayerAnimator* animator,
    ui::LayerAnimationElement::AnimatableProperties animated_property,
    base::TimeDelta duration,
    gfx::Tween::Type tween,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    base::TimeDelta delay) {
  DCHECK_EQ(settings->GetAnimator(), animator);
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(tween);
  settings->SetPreemptionStrategy(preemption_strategy);
  if (!delay.is_zero())
    animator->SchedulePauseForProperties(delay, animated_property);
}

}  // namespace

void DoSplitviewOpacityAnimation(ui::Layer* layer,
                                 SplitviewAnimationType type) {
  float target_opacity = 0.f;
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT:
      target_opacity = 0.f;
      break;
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN:
      target_opacity = kPreviewAreaHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
      target_opacity = kHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT:
      target_opacity = 1.f;
      break;
    default:
      NOTREACHED() << "Not a valid split view opacity animation type.";
      return;
  }

  if (layer->GetTargetOpacity() == target_opacity)
    return;

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &preemption_strategy,
                            &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(&settings, animator,
                         ui::LayerAnimationElement::OPACITY, duration, tween,
                         preemption_strategy, delay);
  layer->SetOpacity(target_opacity);
}

void DoSplitviewTransformAnimation(ui::Layer* layer,
                                   SplitviewAnimationType type,
                                   const gfx::Transform& target_transform) {
  if (layer->GetTargetTransform() == target_transform)
    return;

  switch (type) {
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM:
      break;
    default:
      NOTREACHED() << "Not a valid split view transform type.";
      return;
  }

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &preemption_strategy,
                            &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(&settings, animator,
                         ui::LayerAnimationElement::TRANSFORM, duration, tween,
                         preemption_strategy, delay);
  layer->SetTransform(target_transform);
}

void MaybeRestoreSplitView(bool refresh_snapped_windows) {
  if (!ShouldAllowSplitView())
    return;

  // Search for snapped windows to detect if the now active user session, or
  // desk were in split view. In case multiple windows were snapped to one side,
  // one window after another, there may be multiple windows in a LEFT_SNAPPED
  // state or multiple windows in a RIGHT_SNAPPED state. For each of those two
  // state types that belongs to multiple windows, the relevant window will be
  // listed first among those windows, and a null check in the loop body below
  // will filter out the rest of them.
  // TODO(amusbach): The windows that were in split view may have later been
  // destroyed or changed to non-snapped states. Then the following for loop
  // could snap windows that were not in split view. Also, a window may have
  // become full screen, and if so, then it would be better not to reactivate
  // split view. See https://crbug.com/944134.
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();

  if (refresh_snapped_windows) {
    const MruWindowTracker::WindowList windows =
        Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
            kActiveDesk);
    for (aura::Window* window : windows) {
      switch (WindowState::Get(window)->GetStateType()) {
        case WindowStateType::kLeftSnapped:
          if (!split_view_controller->left_window()) {
            split_view_controller->SnapWindow(window,
                                              SplitViewController::LEFT);
          }
          break;

        case WindowStateType::kRightSnapped:
          if (!split_view_controller->right_window()) {
            split_view_controller->SnapWindow(window,
                                              SplitViewController::RIGHT);
          }
          break;

        default:
          break;
      }

      if (split_view_controller->state() == SplitViewState::kBothSnapped)
        break;
    }
  }

  // Ensure that overview mode is active if and only if there is a window
  // snapped to one side but no window snapped to the other side.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewState state = split_view_controller->state();
  if (state == SplitViewState::kLeftSnapped ||
      state == SplitViewState::kRightSnapped) {
    overview_controller->StartOverview();
  } else {
    overview_controller->EndOverview();
  }
}

bool IsClamshellSplitViewModeEnabled() {
  return base::FeatureList::IsEnabled(
      ash::features::kDragToSnapInClamshellMode);
}

bool AreMultiDisplayOverviewAndSplitViewEnabled() {
  return base::FeatureList::IsEnabled(
      ash::features::kMultiDisplayOverviewAndSplitView);
}

bool ShouldAllowSplitView() {
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !IsClamshellSplitViewModeEnabled()) {
    return false;
  }

  // Don't allow split view if we're in pinned mode.
  if (Shell::Get()->screen_pinning_controller()->IsPinned())
    return false;

  // TODO(crubg.com/853588): Disallow window dragging and split screen while
  // ChromeVox is on until they are in a usable state.
  if (Shell::Get()->accessibility_controller()->spoken_feedback_enabled())
    return false;

  return true;
}

bool CanSnapInSplitview(aura::Window* window) {
  if (!ShouldAllowSplitView())
    return false;

  if (!wm::CanActivateWindow(window))
    return false;

  if (!WindowState::Get(window)->CanSnap())
    return false;

  if (window->delegate()) {
    // If the window's minimum size is larger than half of the display's work
    // area size, the window can't be snapped in this case.
    const gfx::Size min_size = window->delegate()->GetMinimumSize();
    const gfx::Rect display_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            window);
    const bool is_landscape = (display_area.width() > display_area.height());
    if ((is_landscape && min_size.width() > display_area.width() / 2) ||
        (!is_landscape && min_size.height() > display_area.height() / 2)) {
      return false;
    }
  }

  return true;
}

void ShowAppCannotSnapToast() {
  ash::Shell::Get()->toast_manager()->Show(ash::ToastData(
      kAppCannotSnapToastId,
      l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP),
      kAppCannotSnapToastDurationMs, base::Optional<base::string16>()));
}

bool IsPhysicalLeftOrTop(SplitViewController::SnapPosition position) {
  DCHECK_NE(SplitViewController::NONE, position);
  return position == (IsCurrentScreenOrientationPrimary()
                          ? SplitViewController::LEFT
                          : SplitViewController::RIGHT);
}

}  // namespace ash
