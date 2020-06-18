// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"

namespace ash {

using AmbientPhotoViewTest = AmbientAshTestBase;

// TODO(b/158617675): test is flaky.
// Test that image is scaled to fill full screen when image and display are in
// the same orientation.
TEST_F(AmbientPhotoViewTest, DISABLED_ShouldResizeImageToFillFullScreen) {
  // Start Ambient mode.
  ShowAmbientScreen();
  // Fastforward enough time to update the photo.
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);

  UpdateDisplay("600x800");
  const auto& image = GetImageInPhotoView();
  // Test image size in test is "10x20". Scaled image should fill the full
  // screen.
  // The expected size is "600x1200".
  EXPECT_EQ(image.size().width(), 600);
  EXPECT_EQ(image.size().height(), 1200);
}

// TODO(b/158617675): test is flaky.
// Test that image is scaled to fill one direction of the screen when image and
// display are in different orientations.
TEST_F(AmbientPhotoViewTest,
       DISABLED_ShouldResizeImageToFillOneDirectionOfScreen) {
  // Start Ambient mode.
  ShowAmbientScreen();
  // Fastforward enough time to update the photo.
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);

  UpdateDisplay("800x600");
  const auto& image = GetImageInPhotoView();
  // Test image size in test is "10x20". Scaled image should fill one direction
  // of the screen.
  // The expected size is "300x600".
  EXPECT_EQ(image.size().width(), 300);
  EXPECT_EQ(image.size().height(), 600);
}

}  // namespace ash
