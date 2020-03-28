// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_PHOTO_VIEW_H_
#define ASH_AMBIENT_UI_PHOTO_VIEW_H_

#include "ash/ambient/model/photo_model_observer.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AmbientBackgroundImageView;
class AmbientViewDelegate;

// View to display photos in ambient mode.
class ASH_EXPORT PhotoView : public views::View, public PhotoModelObserver {
 public:
  explicit PhotoView(AmbientViewDelegate* delegate);
  ~PhotoView() override;

  // views::View:
  const char* GetClassName() const override;
  void AddedToWidget() override;

  // PhotoModelObserver:
  void OnImagesChanged() override;

 private:
  void Init();
  void UpdateImages();
  void StartSlideAnimation();
  bool CanAnimate() const;

  // Note that we should be careful when using |delegate_|, as there is no
  // strong guarantee on the life cycle, especially given that the widget |this|
  // lived in is destroyed asynchronously.
  AmbientViewDelegate* delegate_ = nullptr;

  // Image containers used for animation. Owned by view hierarchy.
  AmbientBackgroundImageView* image_view_prev_ = nullptr;
  AmbientBackgroundImageView* image_view_curr_ = nullptr;
  AmbientBackgroundImageView* image_view_next_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PhotoView);
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_PHOTO_VIEW_H_
