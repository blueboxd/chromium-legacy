// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CLIENT_H_

#include <memory>

#include "ash/public/cpp/ambient/photo_controller.h"
#include "base/callback_forward.h"
#include "base/optional.h"

// The interface of a client to retrieve photos.
class PhotoClient {
 public:
  // Creates PhotoClient based on the build flag ENABLE_CROS_LIBASSISTANT.
  static std::unique_ptr<PhotoClient> Create();

  PhotoClient() = default;
  PhotoClient(const PhotoClient&) = delete;
  PhotoClient& operator=(const PhotoClient&) = delete;
  virtual ~PhotoClient() = default;

  // Sends request to retrieve |ScreenUpdate| from the backdrop server.
  // Upon completion, |callback| is run with the parsed |ScreenUpdate|. If any
  // errors happened during the process, e.g. failed to fetch access token, a
  // dummy instance will be returned.
  using OnScreenUpdateInfoFetchedCallback =
      base::OnceCallback<void(const ash::PhotoController::ScreenUpdate&)>;
  virtual void FetchScreenUpdateInfo(
      OnScreenUpdateInfoFetchedCallback callback);

  virtual void GetSettings(ash::PhotoController::GetSettingsCallback callback);

  virtual void UpdateSettings(
      int topic_source,
      ash::PhotoController::UpdateSettingsCallback callback);
};

#endif  // CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CLIENT_H_
