// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Class to handle photos in ambient mode.
class ASH_EXPORT AmbientPhotoController {
 public:
  // Start fetching next |ScreenUpdate| from the backdrop server. The specified
  // download callback will be run upon completion and returns a null image
  // if: 1. the response did not have the desired fields or urls or, 2. the
  // download attempt from that url failed. The |icon_callback| also returns
  // the weather temperature in Fahrenheit together with the image.
  using PhotoDownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;
  using WeatherIconDownloadCallback =
      base::OnceCallback<void(base::Optional<float>, const gfx::ImageSkia&)>;

  AmbientPhotoController();
  ~AmbientPhotoController();

  void GetNextScreenUpdateInfo(PhotoDownloadCallback photo_callback,
                               WeatherIconDownloadCallback icon_callback);

 private:
  friend class AmbientAshTestBase;

  void OnNextScreenUpdateInfoFetched(PhotoDownloadCallback photo_callback,
                                     WeatherIconDownloadCallback icon_callback,
                                     const ash::ScreenUpdate& screen_update);

  void StartDownloadingPhotoImage(const ash::ScreenUpdate& screen_update,
                                  PhotoDownloadCallback photo_callback);

  void StartDownloadingWeatherConditionIcon(
      const ash::ScreenUpdate& screen_update,
      WeatherIconDownloadCallback icon_callback);

  base::WeakPtrFactory<AmbientPhotoController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AmbientPhotoController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
