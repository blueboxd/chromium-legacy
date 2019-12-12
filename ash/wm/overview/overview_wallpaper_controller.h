// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ui {
class LayerTreeOwner;
class Window;
}  // namespace ui

namespace ash {

// Class that handles of blurring and dimming wallpaper upon entering and
// exiting overview mode. Blurs the wallpaper automatically if the wallpaper is
// not visible prior to entering overview mode (covered by a window), otherwise
// animates the blur and dim.
// TODO(oshima): Move the crosfade animation into WallpaperWidgetController
// this class will no longer be necessary.
class ASH_EXPORT OverviewWallpaperController
    : public aura::WindowObserver,
      public ui::ImplicitAnimationObserver {
 public:
  OverviewWallpaperController();
  ~OverviewWallpaperController() override;

  // There is no need to blur or dim the wallpaper for tests.
  static void SetDoNotChangeWallpaperForTests();

  void Blur(bool animate_only);
  void Unblur();

  bool has_blur() const { return state_ != WallpaperAnimationState::kNormal; }

  bool HasBlurAnimationForTesting() const;
  void StopBlurAnimationsForTesting();

 private:
  enum class WallpaperAnimationState {
    kAddingBlur,
    kRemovingBlur,
    kNormal,
  };

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Called when the wallpaper is to be changed. Checks to see which root
  // windows should have their wallpaper blurs animated and fills
  // |roots_to_animate_| or |blur_layers_| accordingly. Applies blur or unblur
  // immediately if the wallpaper does not need blur animation. When
  // |animate_only| is true, it'll apply blur only to the root windows that
  // requires animation.
  void OnBlurChange(WallpaperAnimationState state, bool animate_only);

  WallpaperAnimationState state_ = WallpaperAnimationState::kNormal;
  // Vector which contains the root windows, if any, whose wallpaper should have
  // blur animated after Blur or Unblur is called.
  std::vector<aura::Window*> roots_to_animate_;

  // Vector that contains the copied layers, one per root window. This should
  // be empty when overview enter animation is not running.
  std::vector<std::unique_ptr<ui::LayerTreeOwner>> animating_copies_;

  DISALLOW_COPY_AND_ASSIGN(OverviewWallpaperController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WALLPAPER_CONTROLLER_H_
