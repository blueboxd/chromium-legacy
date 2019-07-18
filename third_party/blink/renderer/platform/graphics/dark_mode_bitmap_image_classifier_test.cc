// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_bitmap_image_classifier.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace {
const float kEpsilon = 0.00001;

}  // namespace

namespace blink {

class FakeImageForCacheTest : public Image {
 public:
  static scoped_refptr<FakeImageForCacheTest> Create() {
    return base::AdoptRef(new FakeImageForCacheTest());
  }

  int GetMapSize() { return dark_mode_classifications_.size(); }

  DarkModeClassification GetClassification(const FloatRect& src_rect) {
    return GetDarkModeClassification(src_rect);
  }

  void AddClassification(
      const FloatRect& src_rect,
      const DarkModeClassification dark_mode_classification) {
    AddDarkModeClassification(src_rect, dark_mode_classification);
  }

  // Pure virtual functions that have to be overridden.
  bool CurrentFrameKnownToBeOpaque() override { return false; }
  IntSize Size() const override { return IntSize(0, 0); }
  void DestroyDecodedData() override {}
  PaintImage PaintImageForCurrentFrame() override { return PaintImage(); }
  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override {}
};

class DarkModeBitmapImageClassifierTest : public testing::Test {
 public:
  // Loads the image from |file_name|, computes features vector into |features|,
  // and returns the classification result.
  bool GetFeaturesAndClassification(const String& file_name,
                                    Vector<float>* features) {
    SCOPED_TRACE(file_name);
    scoped_refptr<BitmapImage> image = LoadImage(file_name);
    classifier_.ComputeImageFeaturesForTesting(*image.get(), features);
    DarkModeClassification result = classifier_.Classify(
        *image.get(), FloatRect(0, 0, image->width(), image->height()));
    return result == DarkModeClassification::kApplyFilter;
  }

  void AssertFeaturesEqual(const Vector<float>& features,
                           const Vector<float>& expected_features) {
    EXPECT_EQ(features.size(), expected_features.size());
    for (unsigned i = 0; i < features.size(); i++) {
      EXPECT_NEAR(features[i], expected_features[i], kEpsilon)
          << "Feature " << i;
    }
  }

  DarkModeBitmapImageClassifier* classifier() { return &classifier_; }

 protected:
  scoped_refptr<BitmapImage> LoadImage(const String& file_name) {
    String file_path = test::BlinkWebTestsDir() + file_name;
    scoped_refptr<SharedBuffer> image_data = test::ReadFromFile(file_path);
    EXPECT_TRUE(image_data.get() && image_data.get()->size());

    scoped_refptr<BitmapImage> image = BitmapImage::Create();
    image->SetData(image_data, true);
    return image;
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  DarkModeBitmapImageClassifier classifier_;
};

TEST_F(DarkModeBitmapImageClassifierTest, FeaturesAndClassification) {
  Vector<float> features;

  // Test Case 1:
  // Grayscale
  // Color Buckets Ratio: Low
  // Decision Tree: Apply
  // Neural Network: NA
  EXPECT_TRUE(GetFeaturesAndClassification("/images/resources/grid-large.png",
                                           &features));
  EXPECT_EQ(classifier()->ClassifyImageUsingDecisionTreeForTesting(features),
            DarkModeClassification::kApplyFilter);
  AssertFeaturesEqual(features, {0.0f, 0.1875f, 0.0f, 0.0f});

  // Test Case 2:
  // Grayscale
  // Color Buckets Ratio: Medium
  // Decision Tree: Can't Decide
  // Neural Network: Apply
  EXPECT_FALSE(GetFeaturesAndClassification("/images/resources/apng08-ref.png",
                                            &features));
  EXPECT_EQ(classifier()->ClassifyImageUsingDecisionTreeForTesting(features),
            DarkModeClassification::kNotClassified);
  AssertFeaturesEqual(features, {0.0f, 0.8125f, 0.446667f, 0.03f});

  // Test Case 3:
  // Color
  // Color Buckets Ratio: Low
  // Decision Tree: Apply
  // Neural Network: NA.
  EXPECT_TRUE(GetFeaturesAndClassification(
      "/images/resources/twitter_favicon.ico", &features));
  EXPECT_EQ(classifier()->ClassifyImageUsingDecisionTreeForTesting(features),
            DarkModeClassification::kApplyFilter);
  AssertFeaturesEqual(features, {1.0f, 0.0002441f, 0.542092f, 0.1500000f});

  // Test Case 4:
  // Color
  // Color Buckets Ratio: High
  // Decision Tree: Do Not Apply
  // Neural Network: NA.
  EXPECT_FALSE(GetFeaturesAndClassification(
      "/images/resources/blue-wheel-srgb-color-profile.png", &features));
  EXPECT_EQ(classifier()->ClassifyImageUsingDecisionTreeForTesting(features),
            DarkModeClassification::kDoNotApplyFilter);
  AssertFeaturesEqual(features, {1.0f, 0.032959f, 0.0f, 0.0f});

  // Test Case 5:
  // Color
  // Color Buckets Ratio: Medium
  // Decision Tree: Apply
  // Neural Network: NA.
  EXPECT_TRUE(GetFeaturesAndClassification(
      "/images/resources/ycbcr-444-float.jpg", &features));
  EXPECT_EQ(classifier()->ClassifyImageUsingDecisionTreeForTesting(features),
            DarkModeClassification::kApplyFilter);
  AssertFeaturesEqual(features, {1.0f, 0.0151367f, 0.0f, 0.0f});
}

TEST_F(DarkModeBitmapImageClassifierTest, Caching) {
  scoped_refptr<FakeImageForCacheTest> image = FakeImageForCacheTest::Create();
  FloatRect src_rect1(0, 0, 50, 50);
  FloatRect src_rect2(5, 20, 100, 100);
  FloatRect src_rect3(6, -9, 50, 50);

  EXPECT_EQ(image->GetClassification(src_rect1),
            DarkModeClassification::kNotClassified);
  image->AddClassification(src_rect1, DarkModeClassification::kApplyFilter);
  EXPECT_EQ(image->GetClassification(src_rect1),
            DarkModeClassification::kApplyFilter);

  EXPECT_EQ(image->GetClassification(src_rect2),
            DarkModeClassification::kNotClassified);
  image->AddClassification(src_rect2,
                           DarkModeClassification::kDoNotApplyFilter);
  EXPECT_EQ(image->GetClassification(src_rect2),
            DarkModeClassification::kDoNotApplyFilter);

  EXPECT_EQ(image->GetClassification(src_rect3),
            DarkModeClassification::kNotClassified);
  image->AddClassification(src_rect3, DarkModeClassification::kApplyFilter);
  EXPECT_EQ(image->GetClassification(src_rect3),
            DarkModeClassification::kApplyFilter);

  EXPECT_EQ(image->GetMapSize(), 3);
}

}  // namespace blink
