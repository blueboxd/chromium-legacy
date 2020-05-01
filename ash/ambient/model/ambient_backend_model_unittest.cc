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

namespace {

// This class has a local in memory cache of downloaded photos. This is the max
// number of photos before and after currently shown image.
constexpr int kTestImageBufferLength = 3;

}  // namespace

class AmbientBackendModelTest : public AshTestBase {
 public:
  AmbientBackendModelTest() = default;
  AmbientBackendModelTest(const AmbientBackendModelTest&) = delete;
  AmbientBackendModelTest& operator=(AmbientBackendModelTest&) = delete;
  ~AmbientBackendModelTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    ambient_backend_model_ = std::make_unique<AmbientBackendModel>();
    ambient_backend_model_->set_buffer_length_for_testing(
        kTestImageBufferLength);
  }

  void TearDown() override {
    ambient_backend_model_.reset();
    AshTestBase::TearDown();
  }

  void ShowNextImage() { ambient_backend_model_->ShowNextImage(); }

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

  gfx::ImageSkia prev_image() { return ambient_backend_model_->GetPrevImage(); }

  gfx::ImageSkia curr_image() { return ambient_backend_model_->GetCurrImage(); }

  gfx::ImageSkia next_image() { return ambient_backend_model_->GetNextImage(); }

 private:
  std::unique_ptr<AmbientBackendModel> ambient_backend_model_;
};

// Test adding the first image.
TEST_F(AmbientBackendModelTest, AddFirstImage) {
  AddNTestImages(1);

  EXPECT_TRUE(IsNullImage(prev_image()));
  EXPECT_TRUE(EqualsToTestImage(curr_image()));
  EXPECT_TRUE(IsNullImage(next_image()));
}

// Test adding the second image.
TEST_F(AmbientBackendModelTest, AddSecondImage) {
  AddNTestImages(2);

  // The default |current_image_index_| is 0.
  EXPECT_TRUE(IsNullImage(prev_image()));
  EXPECT_TRUE(EqualsToTestImage(curr_image()));
  EXPECT_TRUE(EqualsToTestImage(next_image()));

  // Increment the |current_image_index_| to 1.
  ShowNextImage();
  EXPECT_TRUE(EqualsToTestImage(prev_image()));
  EXPECT_TRUE(EqualsToTestImage(curr_image()));
  EXPECT_TRUE(IsNullImage(next_image()));
}

// Test adding the third image.
TEST_F(AmbientBackendModelTest, AddThirdImage) {
  AddNTestImages(3);

  // The default |current_image_index_| is 0.
  EXPECT_TRUE(IsNullImage(prev_image()));
  EXPECT_TRUE(EqualsToTestImage(curr_image()));
  EXPECT_TRUE(EqualsToTestImage(next_image()));

  // Increment the |current_image_index_| to 1.
  ShowNextImage();
  EXPECT_TRUE(EqualsToTestImage(prev_image()));
  EXPECT_TRUE(EqualsToTestImage(curr_image()));
  EXPECT_TRUE(EqualsToTestImage(next_image()));

  // Pop the |images_| front and keep the |current_image_index_| to 1.
  ShowNextImage();
  EXPECT_TRUE(EqualsToTestImage(prev_image()));
  EXPECT_TRUE(EqualsToTestImage(curr_image()));
  EXPECT_TRUE(IsNullImage(next_image()));

  // ShowNextImage() will early return.
  ShowNextImage();
  EXPECT_TRUE(EqualsToTestImage(prev_image()));
  EXPECT_TRUE(EqualsToTestImage(curr_image()));
  EXPECT_TRUE(IsNullImage(next_image()));
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
