// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/image_view.h"

namespace ash {

using AmbientPhotoViewTest = AmbientAshTestBase;

// Test that image is scaled to fill screen width when image is portrait and
// screen is portrait. The top and bottom of the image will be cut off, as
// the height of the image is taller than the height of the screen.
TEST_F(AmbientPhotoViewTest, ShouldResizePortraitImageForPortraitScreen) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("600x800");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Image should be full width. Image height should extend above and below the
  // visible part of the screen.
  ASSERT_EQ(image_view->GetCurrentImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/-200, /*width=*/600, /*height=*/1200));
}

// Test that image is scaled to fill screen width when the image is landscape
// and the screen is portrait. There will be black bars to the top and bottom of
// the image, as the height of the image is less than the height of the screen.
TEST_F(AmbientPhotoViewTest, ShouldResizeLandscapeImageForPortraitScreen) {
  SetPhotoViewImageSize(/*width=*/30, /*height=*/20);

  UpdateDisplay("600x800");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Image should be full width. Image should have equal empty space top and
  // bottom.
  ASSERT_EQ(image_view->GetCurrentImageBoundsForTesting(),
            gfx::Rect(/*x=*/0, /*y=*/200, /*width=*/600, /*height=*/400));
}

// Test that image is scaled to fill screen height when the image is portrait
// and screen is landscape. There will be black bars to the left and right of
// the image, as the width of the image is less than the width of the screen.
TEST_F(AmbientPhotoViewTest, ShouldResizePortraitImageForLandscapeScreen) {
  SetPhotoViewImageSize(/*width=*/10, /*height=*/20);

  UpdateDisplay("800x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Image should be full height. Image width should have equal empty space on
  // left and right.
  ASSERT_EQ(image_view->GetCurrentImageBoundsForTesting(),
            gfx::Rect(/*x=*/250, /*y=*/0, /*width=*/300, /*height=*/600));
}

// Test that image is scaled to fill screen height when the image is landscape
// and the screen is landscape. The image will be zoomed in and the left and
// right will be cut off, as the width of the image is greater than the width of
// the screen.
TEST_F(AmbientPhotoViewTest, ShouldResizeLandscapeImageForFillLandscapeScreen) {
  SetPhotoViewImageSize(/*width=*/30, /*height=*/20);

  UpdateDisplay("800x600");

  ShowAmbientScreen();

  FastForwardToNextImage();

  auto* image_view = GetAmbientBackgroundImageView();

  // Image should be full height. Image width should extend equally to the left
  // and right of the visible part of the screen.
  ASSERT_EQ(image_view->GetCurrentImageBoundsForTesting(),
            gfx::Rect(/*x=*/-50, /*y=*/0, /*width=*/900, /*height=*/600));
}

}  // namespace ash
