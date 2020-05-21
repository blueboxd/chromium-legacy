// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_backend_model.h"

#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class AmbientBackendModelTest : public AshTestBase {
 public:
  AmbientBackendModelTest() = default;
  AmbientBackendModelTest(const AmbientBackendModelTest&) = delete;
  AmbientBackendModelTest& operator=(AmbientBackendModelTest&) = delete;
  ~AmbientBackendModelTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ambient_backend_model_ = std::make_unique<AmbientBackendModel>();
  }

  void TearDown() override {
    ambient_backend_model_.reset();
    AshTestBase::TearDown();
  }

  // Adds n test images to the model.
  void AddNTestImages(int n) {
    while (n > 0) {
      gfx::ImageSkia test_image =
          gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
      ambient_backend_model()->AddNextImage(test_image);
      n--;
    }
  }

  // Returns whether the image is equivalent to the test image.
  bool EqualsToTestImage(const gfx::ImageSkia& image) {
    gfx::ImageSkia test_image =
        gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
    return !image.isNull() &&
           gfx::test::AreBitmapsEqual(*image.bitmap(), *test_image.bitmap());
  }

  // Returns whether the image is null.
  bool IsNullImage(const gfx::ImageSkia& image) { return image.isNull(); }

  base::TimeDelta GetPhotoRefreshInterval() {
    return ambient_backend_model()->GetPhotoRefreshInterval();
  }

  void SetPhotoRefreshInterval(const base::TimeDelta& interval) {
    ambient_backend_model()->SetPhotoRefreshInterval(interval);
  }

  AmbientBackendModel* ambient_backend_model() {
    return ambient_backend_model_.get();
  }

  gfx::ImageSkia current_image() {
    return ambient_backend_model_->GetCurrentImage();
  }

  gfx::ImageSkia next_image() { return ambient_backend_model_->GetNextImage(); }

 private:
  std::unique_ptr<AmbientBackendModel> ambient_backend_model_;
};

// Test adding the first image.
TEST_F(AmbientBackendModelTest, AddFirstImage) {
  AddNTestImages(1);

  EXPECT_TRUE(EqualsToTestImage(current_image()));
  EXPECT_TRUE(IsNullImage(next_image()));
}

// Test adding the second image.
TEST_F(AmbientBackendModelTest, AddSecondImage) {
  AddNTestImages(2);

  EXPECT_TRUE(EqualsToTestImage(current_image()));
  EXPECT_TRUE(EqualsToTestImage(next_image()));
}

// Test adding the third image.
TEST_F(AmbientBackendModelTest, AddThirdImage) {
  AddNTestImages(2);

  EXPECT_TRUE(EqualsToTestImage(current_image()));
  EXPECT_TRUE(EqualsToTestImage(next_image()));

  auto previous_next_image = next_image();
  AddNTestImages(1);
  EXPECT_TRUE(EqualsToTestImage(current_image()));
  EXPECT_TRUE(EqualsToTestImage(next_image()));
  EXPECT_TRUE(previous_next_image.BackedBySameObjectAs(current_image()));
}

// Test the photo refresh interval is expected.
TEST_F(AmbientBackendModelTest, ShouldReturnExpectedPhotoRefreshInterval) {
  // Should fetch image immediately.
  EXPECT_EQ(GetPhotoRefreshInterval(), base::TimeDelta());

  AddNTestImages(1);
  // Should fetch image immediately.
  EXPECT_EQ(GetPhotoRefreshInterval(), base::TimeDelta());

  AddNTestImages(1);
  // Has enough images. Will fetch more image at the |photo_refresh_interval_|,
  // which is |kPhotoRefreshInterval| by default.
  EXPECT_EQ(GetPhotoRefreshInterval(), kPhotoRefreshInterval);

  // Change the photo refresh interval.
  const base::TimeDelta interval = base::TimeDelta::FromMinutes(1);
  SetPhotoRefreshInterval(interval);
  // The refresh interval will be the set value.
  EXPECT_EQ(GetPhotoRefreshInterval(), interval);
}

}  // namespace ash
