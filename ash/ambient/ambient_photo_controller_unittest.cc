// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

using AmbientPhotoControllerTest = AmbientAshTestBase;

// Test that topics are downloaded when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldStartToDownloadTopics) {
  auto topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_TRUE(topics.empty());

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_TRUE(topics.empty());

  task_environment()->FastForwardBy(kPhotoRefreshInterval);
  topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_FALSE(topics.empty());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  topics = photo_controller()->ambient_backend_model()->topics();
  EXPECT_TRUE(topics.empty());
}

// Test that image is downloaded when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldStartToDownloadImages) {
  auto image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_TRUE(image.isNull());

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  base::RunLoop().RunUntilIdle();
  image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_FALSE(image.isNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  image = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_TRUE(image.isNull());
}

// Tests that photos are updated periodically when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldUpdatePhotoPeriodically) {
  gfx::ImageSkia image1;
  gfx::ImageSkia image2;
  gfx::ImageSkia image3;

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  base::RunLoop().RunUntilIdle();
  image1 = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_FALSE(image1.isNull());
  EXPECT_TRUE(image2.isNull());

  // Fastforward enough time to update the photo.
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);
  image2 = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_FALSE(image2.isNull());
  EXPECT_FALSE(image1.BackedBySameObjectAs(image2));
  EXPECT_TRUE(image3.isNull());

  // Fastforward enough time to update another photo.
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);
  image3 = photo_controller()->ambient_backend_model()->GetCurrentImage();
  EXPECT_FALSE(image3.isNull());
  EXPECT_FALSE(image1.BackedBySameObjectAs(image3));
  EXPECT_FALSE(image2.BackedBySameObjectAs(image3));
}

}  // namespace ash
