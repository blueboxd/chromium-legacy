// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ambient/photo_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/ash/ambient/photo_client.h"

// TODO(wutao): Move this class to ash.
// Class to handle photos from Backdrop service.
class PhotoControllerImpl : public ash::PhotoController {
 public:
  PhotoControllerImpl();
  ~PhotoControllerImpl() override;

  // ash::PhotoController:
  void GetNextScreenUpdateInfo(
      PhotoDownloadCallback photo_callback,
      WeatherIconDownloadCallback icon_callback) override;
  void GetSettings(GetSettingsCallback callback) override;
  void UpdateSettings(int topic_source,
                      UpdateSettingsCallback callback) override;

 private:
  void StartDownloadingPhotoImage(
      const ash::PhotoController::ScreenUpdate& screen_update,
      PhotoDownloadCallback photo_callback);
  void StartDownloadingWeatherConditionIcon(
      const ash::PhotoController::ScreenUpdate& screen_update,
      WeatherIconDownloadCallback icon_callback);

  void OnNextScreenUpdateInfoFetched(
      PhotoDownloadCallback photo_callback,
      WeatherIconDownloadCallback icon_callback,
      const ash::PhotoController::ScreenUpdate& screen_update);

  std::unique_ptr<PhotoClient> photo_client_;

  base::WeakPtrFactory<PhotoControllerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PhotoControllerImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
