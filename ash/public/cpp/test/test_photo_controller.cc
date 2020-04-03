// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_photo_controller.h"

#include <utility>

#include "ash/public/cpp/ambient/photo_controller.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

TestPhotoController::TestPhotoController() = default;
TestPhotoController::~TestPhotoController() = default;

void TestPhotoController::GetNextImage(
    PhotoController::PhotoDownloadCallback callback) {
  gfx::ImageSkia image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  std::move(callback).Run(/*success=*/true, image);
}

void TestPhotoController::GetSettings(GetSettingsCallback callback) {
  // 0 is the enum number for Google Photos.
  std::move(callback).Run(/*topic_source=*/0);
}

void TestPhotoController::UpdateSettings(int topic_source,
                                         UpdateSettingsCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

}  // namespace ash
