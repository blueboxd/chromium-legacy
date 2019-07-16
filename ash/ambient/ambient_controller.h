// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_CONTROLLER_H_

#include "ash/ambient/model/photo_model.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientContainerView;
class PhotoModelObserver;

// Class to handle all ambient mode functionalities.
class ASH_EXPORT AmbientController : views::WidgetObserver {
 public:
  AmbientController();
  ~AmbientController() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void Toggle();
  void AddPhotoModelObserver(PhotoModelObserver* observer);
  void RemovePhotoModelObserver(PhotoModelObserver* observer);
  AmbientContainerView* GetAmbientContainerViewForTesting();

 private:
  void Start();
  void Stop();
  void CreateContainerView();
  void DestroyContainerView();
  void RefreshImage();
  void OnPhotoDownloaded(const gfx::ImageSkia& image);

  AmbientContainerView* container_view_ = nullptr;
  PhotoModel model_;
  base::OneShotTimer refresh_timer_;
  base::WeakPtrFactory<AmbientController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AmbientController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONTROLLER_H_
