// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_wallpaper_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/aura/window_tree_host.h"

namespace ash {

namespace {

// Do not change the wallpaper when entering or exiting overview mode when this
// is true.
bool g_disable_wallpaper_change_for_tests = false;

constexpr int kBlurSlideDurationMs = 250;

bool IsWallpaperChangeAllowed() {
  return !g_disable_wallpaper_change_for_tests &&
         Shell::Get()->wallpaper_controller()->IsBlurAllowed();
}

}  // namespace

OverviewWallpaperController::OverviewWallpaperController() = default;

OverviewWallpaperController::~OverviewWallpaperController() {
  if (compositor_)
    compositor_->RemoveAnimationObserver(this);
  for (aura::Window* root : roots_to_animate_)
    root->RemoveObserver(this);
}

// static
void OverviewWallpaperController::SetDoNotChangeWallpaperForTests() {
  g_disable_wallpaper_change_for_tests = true;
}

void OverviewWallpaperController::Blur(bool animate_only) {
  if (!IsWallpaperChangeAllowed())
    return;
  OnBlurChange(WallpaperAnimationState::kAddingBlur, animate_only);
}

void OverviewWallpaperController::Unblur() {
  if (!IsWallpaperChangeAllowed())
    return;
  OnBlurChange(WallpaperAnimationState::kRemovingBlur,
               /*animate_only=*/false);
}

void OverviewWallpaperController::Stop() {
  if (compositor_) {
    compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
  state_ = WallpaperAnimationState::kNormal;
}

void OverviewWallpaperController::Start() {
  DCHECK(!compositor_);
  compositor_ = Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  compositor_->AddAnimationObserver(this);
  start_time_ = base::TimeTicks();
}

void OverviewWallpaperController::AnimationProgressed(float value) {
  // Animate only to even numbers to reduce the load.
  int ivalue = static_cast<int>(value * kWallpaperBlurSigma) / 2 * 2;
  for (aura::Window* root : roots_to_animate_)
    ApplyBlurAndOpacity(root, ivalue);
}

void OverviewWallpaperController::OnAnimationStep(base::TimeTicks timestamp) {
  if (start_time_ == base::TimeTicks()) {
    start_time_ = timestamp;
    return;
  }
  const float progress = (timestamp - start_time_).InMilliseconds() /
                         static_cast<float>(kBlurSlideDurationMs);
  const bool adding = state_ == WallpaperAnimationState::kAddingBlur;
  if (progress > 1.0f) {
    AnimationProgressed(adding ? 1.0f : 0.f);
    Stop();
  } else {
    AnimationProgressed(adding ? progress : 1.f - progress);
  }
}

void OverviewWallpaperController::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  if (compositor_ == compositor)
    Stop();
}

void OverviewWallpaperController::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  auto it =
      std::find(roots_to_animate_.begin(), roots_to_animate_.end(), window);
  if (it != roots_to_animate_.end())
    roots_to_animate_.erase(it);
}

void OverviewWallpaperController::ApplyBlurAndOpacity(aura::Window* root,
                                                      int value) {
  DCHECK_GE(value, 0);
  DCHECK_LE(value, 10);
  const float opacity =
      gfx::Tween::FloatValueBetween(value / 10.0, 1.f, kShieldOpacity);
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root)->wallpaper_widget_controller();
  if (wallpaper_widget_controller->wallpaper_view()) {
    wallpaper_widget_controller->wallpaper_view()->RepaintBlurAndOpacity(
        value, opacity);
  }
}

void OverviewWallpaperController::OnBlurChange(WallpaperAnimationState state,
                                               bool animate_only) {
  Stop();
  for (aura::Window* root : roots_to_animate_)
    root->RemoveObserver(this);
  roots_to_animate_.clear();

  state_ = state;
  const bool should_blur = state_ == WallpaperAnimationState::kAddingBlur;
  if (animate_only)
    DCHECK(should_blur);

  const float value =
      should_blur ? kWallpaperBlurSigma : kWallpaperClearBlurSigma;

  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    // No need to animate the blur on exiting as this should only be called
    // after overview animations are finished.
    if (should_blur) {
      DCHECK(overview_session);
      OverviewGrid* grid = overview_session->GetGridWithRootWindow(root);
      bool should_animate = grid && grid->ShouldAnimateWallpaper();
      auto* wallpaper_view = RootWindowController::ForWindow(root)
                                 ->wallpaper_widget_controller()
                                 ->wallpaper_view();
      float blur_sigma = wallpaper_view ? wallpaper_view->repaint_blur() : 0.f;
      if (should_animate && animate_only && blur_sigma != kWallpaperBlurSigma) {
        root->AddObserver(this);
        roots_to_animate_.push_back(root);
        continue;
      }
      if (should_animate == animate_only)
        ApplyBlurAndOpacity(root, value);
    } else {
      ApplyBlurAndOpacity(root, value);
    }
  }

  // Run the animation if one of the roots needs to be animated.
  if (roots_to_animate_.empty())
    state_ = WallpaperAnimationState::kNormal;
  else
    Start();
}

}  // namespace ash
