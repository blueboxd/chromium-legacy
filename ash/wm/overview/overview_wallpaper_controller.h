// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/compositor_animation_observer.h"

namespace ui {
class Compositor;
class Window;
}  // namespace ui

namespace ash {

// Class that handles of blurring and dimming wallpaper upon entering and
// exiting overview mode. Blurs the wallpaper automatically if the wallpaper is
// not visible prior to entering overview mode (covered by a window), otherwise
// animates the blur and dim.
class ASH_EXPORT OverviewWallpaperController
    : public ui::CompositorAnimationObserver,
      public aura::WindowObserver {
 public:
  OverviewWallpaperController();
  ~OverviewWallpaperController() override;

  // There is no need to blur or dim the wallpaper for tests.
  static void SetDoNotChangeWallpaperForTests();

  void Blur(bool animate_only);
  void Unblur();

  bool has_blur() const { return state_ != WallpaperAnimationState::kNormal; }
  bool has_blur_animation() const { return !!compositor_; }

 private:
  enum class WallpaperAnimationState {
    kAddingBlur,
    kRemovingBlur,
    kNormal,
  };

  void Stop();
  void Start();
  void AnimationProgressed(float value);

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  void ApplyBlurAndOpacity(aura::Window* root, int value);

  // Called when the wallpaper is to be changed. Checks to see which root
  // windows should have their wallpaper blurs animated and fills
  // |roots_to_animate_| accordingly. Applies blur or unblur immediately if
  // the wallpaper does not need blur animation.
  // When |animate_only| is true, it'll apply blur only to the root windows that
  // requires animation.
  void OnBlurChange(WallpaperAnimationState state, bool animate_only);

  ui::Compositor* compositor_ = nullptr;
  base::TimeTicks start_time_;

  WallpaperAnimationState state_ = WallpaperAnimationState::kNormal;
  // Vector which contains the root windows, if any, whose wallpaper should have
  // blur animated after Blur or Unblur is called.
  std::vector<aura::Window*> roots_to_animate_;

  DISALLOW_COPY_AND_ASSIGN(OverviewWallpaperController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_
