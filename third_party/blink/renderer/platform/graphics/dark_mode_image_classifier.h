// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CLASSIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CLASSIFIER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

FORWARD_DECLARE_TEST(DarkModeImageClassifierTest, FeaturesAndClassification);
FORWARD_DECLARE_TEST(DarkModeImageClassifierTest, Caching);

// This class is not threadsafe as the cache used for storing classification
// results is not threadsafe. So it can be used only in blink main thread.
class PLATFORM_EXPORT DarkModeImageClassifier {
 public:
  DarkModeImageClassifier();
  ~DarkModeImageClassifier();

  struct Features {
    // True if the image is in color, false if it is grayscale.
    bool is_colorful;

    // Ratio of the number of bucketed colors used in the image to all
    // possibilities. Color buckets are represented with 4 bits per color
    // channel.
    float color_buckets_ratio;

    // How much of the image is transparent or considered part of the
    // background.
    float transparency_ratio;
    float background_ratio;
  };

  // Performance warning: |paint_image| will be synchronously decoded if this
  // function is called in blink main thread.
  DarkModeClassification Classify(const PaintImage& paint_image,
                                  const SkRect& src,
                                  const SkRect& dst);

  // Removes cache identified by given |image_id|.
  static void RemoveCache(PaintImage::Id image_id);

 private:
  DarkModeClassification ClassifyWithFeatures(const Features& features);
  DarkModeClassification ClassifyUsingDecisionTree(const Features& features);
  bool GetBitmap(const PaintImage& paint_image,
                 const SkRect& src,
                 SkBitmap* bitmap);
  base::Optional<Features> GetFeatures(const PaintImage& paint_image,
                                       const SkRect& src);

  enum class ColorMode { kColor = 0, kGrayscale = 1 };

  // Extracts a sample set of pixels (|sampled_pixels|), |transparency_ratio|,
  // and |background_ratio|.
  void GetSamples(const PaintImage& paint_image,
                  const SkRect& src,
                  std::vector<SkColor>* sampled_pixels,
                  float* transparency_ratio,
                  float* background_ratio);

  // Gets the |required_samples_count| for a specific |block| of the given
  // SkBitmap, and returns |sampled_pixels| and |transparent_pixels_count|.
  void GetBlockSamples(const SkBitmap& bitmap,
                       const gfx::Rect& block,
                       const int required_samples_count,
                       std::vector<SkColor>* sampled_pixels,
                       int* transparent_pixels_count);

  // Given |sampled_pixels|, |transparency_ratio|, and |background_ratio| for an
  // image, computes and returns the features required for classification.
  Features ComputeFeatures(const std::vector<SkColor>& sampled_pixels,
                           const float transparency_ratio,
                           const float background_ratio);

  // Receives sampled pixels and color mode, and returns the ratio of color
  // buckets count to all possible color buckets. If image is in color, a color
  // bucket is a 4 bit per channel representation of each RGB color, and if it
  // is grayscale, each bucket is a 4 bit representation of luminance.
  float ComputeColorBucketsRatio(const std::vector<SkColor>& sampled_pixels,
                                 const ColorMode color_mode);

  // Gets cached value from the given |image_id| cache.
  DarkModeClassification GetCacheValue(PaintImage::Id image_id,
                                       const SkRect& src);
  // Adds cache value |result| to the given |image_id| cache.
  void AddCacheValue(PaintImage::Id image_id,
                     const SkRect& src,
                     DarkModeClassification result);
  // Returns the cache size for the given |image_id|.
  size_t GetCacheSize(PaintImage::Id image_id);

  FRIEND_TEST_ALL_PREFIXES(DarkModeImageClassifierTest,
                           FeaturesAndClassification);
  FRIEND_TEST_ALL_PREFIXES(DarkModeImageClassifierTest, Caching);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CLASSIFIER_H_
