// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
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
  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());
}

// Tests that photos are updated periodically when starting screen update.
TEST_F(AmbientPhotoControllerTest, ShouldUpdatePhotoPeriodically) {
  PhotoWithDetails image1;
  PhotoWithDetails image2;
  PhotoWithDetails image3;

  // Start to refresh images.
  photo_controller()->StartScreenUpdate();
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);
  image1 = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image1.IsNull());
  EXPECT_TRUE(image2.IsNull());

  // Fastforward enough time to update the photo.
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);
  image2 = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image2.IsNull());
  EXPECT_FALSE(image1.photo.BackedBySameObjectAs(image2.photo));
  EXPECT_TRUE(image3.IsNull());

  // Fastforward enough time to update another photo.
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);
  image3 = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image3.IsNull());
  EXPECT_FALSE(image1.photo.BackedBySameObjectAs(image3.photo));
  EXPECT_FALSE(image2.photo.BackedBySameObjectAs(image3.photo));

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
}

// Test that image is saved and deleted when starting/stopping screen update.
TEST_F(AmbientPhotoControllerTest, ShouldSaveAndDeleteImagesOnDisk) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);

  base::FilePath ambient_image_path =
      home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));

  // Save a file to check if it gets deleted by StartScreenUpdate.
  auto file_to_delete = ambient_image_path.Append("file_to_delete");
  base::WriteFile(file_to_delete, "delete_me");

  // Start to refresh images. Kicks off tasks that cleans |ambient_image_path|,
  // then downloads a test image and writes it to a subdirectory of
  // |ambient_image_path| in a delayed task.
  photo_controller()->StartScreenUpdate();
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);

  EXPECT_TRUE(base::PathExists(ambient_image_path));
  EXPECT_FALSE(base::PathExists(file_to_delete));

  {
    // Count files and directories in root_path. There should only be one
    // subdirectory that was just created to save image files for this ambient
    // mode session.
    base::FileEnumerator files(
        ambient_image_path, /*recursive=*/false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    int count = 0;
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      EXPECT_TRUE(files.GetInfo().IsDirectory());
      count++;
    }

    EXPECT_EQ(count, 1);
  }

  auto image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_FALSE(image.IsNull());

  // Stop to refresh images.
  photo_controller()->StopScreenUpdate();
  task_environment()->FastForwardBy(1.2 * kPhotoRefreshInterval);

  EXPECT_TRUE(base::PathExists(ambient_image_path));
  EXPECT_TRUE(base::IsDirectoryEmpty(ambient_image_path));

  image = photo_controller()->ambient_backend_model()->GetNextImage();
  EXPECT_TRUE(image.IsNull());
}

}  // namespace ash
