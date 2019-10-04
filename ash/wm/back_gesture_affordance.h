// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_BACK_GESTURE_AFFORDANCE_H_
#define ASH_WM_BACK_GESTURE_AFFORDANCE_H_

#include <memory>

#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"

namespace ash {

// This class is responsible for creating, painting, and positioning the layer
// for the back gesture affordance.
class BackGestureAffordance : public ui::LayerDelegate,
                              public gfx::AnimationDelegate {
 public:
  explicit BackGestureAffordance(const gfx::Point& location);
  ~BackGestureAffordance() override;

  // Sets drag progress. 0 means no progress. 1 means full progress and drag
  // will be completed with full progress. Values more than 1 are also allowed
  // for achieving burst ripple. But the maximium value is limited as
  // |kBurstTransform| / |kMaxTransform|.
  void SetDragProgress(int x_location);

  // Aborts the affordance and animates it back.
  void Abort();

  // Completes the affordance and fading it out.
  void Complete();

  ui::Layer* painted_layer() { return &painted_layer_; }

 private:
  enum class State { DRAGGING, ABORTING, COMPLETING };

  void UpdateTransform();
  void SchedulePaint();
  void SetAbortProgress(float progress);
  void SetCompleteProgress(float progress);

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Layer that actually paints the affordance.
  ui::Layer painted_layer_;

  // Values that determine current state of the affordance.
  State state_ = State::DRAGGING;
  float drag_progress_ = 0.f;
  float abort_progress_ = 0.f;
  float complete_progress_ = 0.f;

  std::unique_ptr<gfx::LinearAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(BackGestureAffordance);
};

}  // namespace ash

#endif  // ASH_WM_BACK_GESTURE_AFFORDANCE_H_
