// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_PHOTO_MODEL_H_
#define ASH_AMBIENT_MODEL_PHOTO_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class PhotoModelObserver;

// The model belonging to AmbientController which tracks photo state and
// notifies a pool of observers.
class PhotoModel {
 public:
  PhotoModel();
  ~PhotoModel();

  void AddObserver(PhotoModelObserver* observer);
  void RemoveObserver(PhotoModelObserver* observer);

  void AddNextImage(const gfx::ImageSkia& image);

 private:
  void NotifyImageAvailable(const gfx::ImageSkia& image);

  base::ObserverList<ash::PhotoModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(PhotoModel);
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_PHOTO_MODEL_H_
