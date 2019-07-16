// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/photo_model.h"

#include "ash/ambient/model/photo_model_observer.h"

namespace ash {

PhotoModel::PhotoModel() = default;

PhotoModel::~PhotoModel() = default;

void PhotoModel::AddObserver(PhotoModelObserver* observer) {
  observers_.AddObserver(observer);
}

void PhotoModel::RemoveObserver(PhotoModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PhotoModel::AddNextImage(const gfx::ImageSkia& image) {
  NotifyImageAvailable(image);
}

void PhotoModel::NotifyImageAvailable(const gfx::ImageSkia& image) {
  for (auto& observer : observers_)
    observer.OnImageAvailable(image);
}

}  // namespace ash
