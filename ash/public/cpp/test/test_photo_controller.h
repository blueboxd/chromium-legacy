// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_PHOTO_CONTROLLER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_PHOTO_CONTROLLER_H_

#include "ash/public/cpp/ambient/photo_controller.h"
#include "base/macros.h"

namespace ash {

class ASH_PUBLIC_EXPORT TestPhotoController : public PhotoController {
 public:
  TestPhotoController();
  TestPhotoController(const TestPhotoController&) = delete;
  TestPhotoController& operator=(TestPhotoController&) = delete;
  ~TestPhotoController() override;

  // PhotoController:
  void GetNextImage(PhotoController::PhotoDownloadCallback callback) override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_PHOTO_CONTROLLER_H_
