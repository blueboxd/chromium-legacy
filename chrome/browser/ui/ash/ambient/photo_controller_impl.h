// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_

#include "ash/public/cpp/ambient/photo_controller.h"
#include "base/macros.h"

// TODO(wutao): Move this class to ash.
// Class to handle photos from Backdrop service.
class PhotoControllerImpl : public ash::PhotoController {
 public:
  PhotoControllerImpl();
  ~PhotoControllerImpl() override;

  // ash::PhotoController:
  void GetNextImage(
      ash::PhotoController::PhotoDownloadCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhotoControllerImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_AMBIENT_PHOTO_CONTROLLER_IMPL_H_
