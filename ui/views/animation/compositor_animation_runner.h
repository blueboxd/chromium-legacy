// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_COMPOSITOR_ANIMATION_RUNNER_H_
#define UI_VIEWS_ANIMATION_COMPOSITOR_ANIMATION_RUNNER_H_

#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/animation/animation_container.h"

namespace views {

// An animation runner based on ui::Compositor.
class CompositorAnimationRunner : public gfx::AnimationRunner,
                                  public ui::CompositorAnimationObserver {
 public:
  explicit CompositorAnimationRunner(ui::Compositor* compositor);
  ~CompositorAnimationRunner() override;

  // gfx::AnimationRunner:
  void Stop() override;

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

 protected:
  // gfx::AnimationRunner:
  void OnStart(base::TimeDelta min_interval, base::TimeDelta elapsed) override;

 private:
  // This observes Compositor's destruction and helps CompositorAnimationRunner
  // to stop animation. we need this because CompositorAnimationRunner observes
  // the Compositor only when it's animating.
  class CompositorChecker : public ui::CompositorObserver {
   public:
    explicit CompositorChecker(CompositorAnimationRunner* runner);
    CompositorChecker(const CompositorChecker& other) = delete;
    CompositorChecker& operator=(const CompositorChecker& other) = delete;
    ~CompositorChecker() override;

    // ui::CompositorObserver:
    void OnCompositingShuttingDown(ui::Compositor* compositor) override;

   private:
    CompositorAnimationRunner* runner_;
    ScopedObserver<ui::Compositor, ui::CompositorObserver> scoped_observer_{
        this};
  };

  // When |compositor_| is nullptr, it means compositor has been shut down.
  ui::Compositor* compositor_;

  base::TimeDelta min_interval_ = base::TimeDelta::Max();
  base::TimeTicks last_tick_;

  CompositorChecker checker_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_COMPOSITOR_ANIMATION_RUNNER_H_
