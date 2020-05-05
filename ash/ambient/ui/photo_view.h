// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_PHOTO_VIEW_H_
#define ASH_AMBIENT_UI_PHOTO_VIEW_H_

#include <memory>

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace ui {
class AnimationMetricsReporter;
}  // namespace ui

namespace ash {

class AmbientBackgroundImageView;
class AmbientViewDelegate;

// View to display photos in ambient mode.
class ASH_EXPORT PhotoView : public views::View,
                             public AmbientBackendModelObserver,
                             public ui::ImplicitAnimationObserver {
 public:
  explicit PhotoView(AmbientViewDelegate* delegate);
  PhotoView(const PhotoView&) = delete;
  PhotoView& operator=(PhotoView&) = delete;
  ~PhotoView() override;

  // views::View:
  const char* GetClassName() const override;
  void AddedToWidget() override;

  // AmbientBackendModelObserver:
  void OnImagesChanged() override;
  void OnWeatherInfoUpdated() override {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  void Init();
  void UpdateImages();
  void StartTransitionAnimation();

  // Return if can start transition animation.
  bool NeedToAnimateTransition() const;

  // Note that we should be careful when using |delegate_|, as there is no
  // strong guarantee on the life cycle, especially given that the widget |this|
  // lived in is destroyed asynchronously.
  AmbientViewDelegate* delegate_ = nullptr;

  std::unique_ptr<ui::AnimationMetricsReporter> metrics_reporter_;

  // Image containers used for animation. Owned by view hierarchy.
  AmbientBackgroundImageView* image_view_prev_ = nullptr;
  AmbientBackgroundImageView* image_view_curr_ = nullptr;
  AmbientBackgroundImageView* image_view_next_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_PHOTO_VIEW_H_
