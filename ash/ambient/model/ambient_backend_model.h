// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_
#define ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_

#include "ash/ash_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class AmbientBackendModelObserver;

// Stores necessary information fetched from the backdrop server to render
// the photo frame and glanceable weather information on Ambient Mode. Owned
// by |AmbientController|.
class ASH_EXPORT AmbientBackendModel {
 public:
  AmbientBackendModel();
  AmbientBackendModel(const AmbientBackendModel&) = delete;
  AmbientBackendModel& operator=(AmbientBackendModel&) = delete;
  ~AmbientBackendModel();

  void AddObserver(AmbientBackendModelObserver* observer);
  void RemoveObserver(AmbientBackendModelObserver* observer);

  // Prefetch one more image for ShowNextImage animations.
  bool ShouldFetchImmediately() const;

  // Show the next downloaded image.
  void ShowNextImage();

  // Add image to local storage.
  void AddNextImage(const gfx::ImageSkia& image);

  // Get/Set the photo refresh interval.
  base::TimeDelta GetPhotoRefreshInterval();
  void SetPhotoRefreshInterval(base::TimeDelta interval);

  // Clear local storage.
  void Clear();

  // Get images from local storage. Could be null image.
  gfx::ImageSkia GetPrevImage() const;
  gfx::ImageSkia GetCurrImage() const;
  gfx::ImageSkia GetNextImage() const;

  // Updates the weather information and notifies observers if the icon image is
  // not null.
  void UpdateWeatherInfo(const gfx::ImageSkia& weather_condition_icon,
                         float temperature);

  // Returns the cached condition icon. Will return a null image if it has not
  // been set yet.
  const gfx::ImageSkia& weather_condition_icon() const {
    return weather_condition_icon_;
  }

  // Returns the cached temperature value in Fahrenheit.
  float temperature() const { return temperature_; }

  void set_buffer_length_for_testing(int length) {
    buffer_length_for_testing_ = length;
  }

 private:
  void NotifyImagesChanged();
  void NotifyWeatherInfoUpdated();

  int GetImageBufferLength() const;

  // A local cache for downloaded images. This buffer is split into two equal
  // length of kImageBufferLength / 2 for previous seen and next unseen images.
  base::circular_deque<gfx::ImageSkia> images_;

  // The index of currently shown image.
  int current_image_index_ = 0;

  // Current weather information. The temperature is in Fahrenheit by default
  // (b/154046129).
  gfx::ImageSkia weather_condition_icon_;
  float temperature_ = 0.0f;

  // The interval to refresh photos.
  base::TimeDelta photo_refresh_interval_;

  base::ObserverList<AmbientBackendModelObserver> observers_;

  int buffer_length_for_testing_ = -1;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_
