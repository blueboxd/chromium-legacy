// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_

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
  using PhotoDownloadCallback = ash::PhotoController::PhotoDownloadCallback;

  PhotoControllerImpl();
  ~PhotoControllerImpl() override;

  // ash::PhotoController:
  void GetNextImage(PhotoDownloadCallback callback) override;

 private:
  void OnNextImageInfoFetched(
      PhotoDownloadCallback callback,
      bool success,
      const base::Optional<ash::PhotoController::Topic>& topic);

  std::unique_ptr<PhotoClient> photo_client_;

  base::WeakPtrFactory<PhotoControllerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PhotoControllerImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
