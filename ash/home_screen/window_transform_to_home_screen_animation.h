// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_WINDOW_TRANSFORM_TO_HOME_SCREEN_ANIMATION_H_
#define ASH_HOME_SCREEN_WINDOW_TRANSFORM_TO_HOME_SCREEN_ANIMATION_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"

namespace gfx {
class Transform;
}

namespace ash {

enum class BackdropWindowMode;

// The class the does the dragged window scale down animation to home screen
// after drag ends. The window will be minimized after animation complete.
class WindowTransformToHomeScreenAnimation
    : public ui::ImplicitAnimationObserver,
      public aura::WindowObserver {
 public:
  WindowTransformToHomeScreenAnimation(
      aura::Window* window,
      base::Optional<BackdropWindowMode> original_backdrop_mode);
  ~WindowTransformToHomeScreenAnimation() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // Returns the transform that should be applied to the dragged window if we
  // should head to homescreen after dragging.
  gfx::Transform GetWindowTransformToHomeScreen();

  aura::Window* window_;
  base::Optional<BackdropWindowMode> original_backdrop_mode_;
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(WindowTransformToHomeScreenAnimation);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_
