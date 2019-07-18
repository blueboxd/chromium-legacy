// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_BITMAP_IMAGE_CLASSIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_BITMAP_IMAGE_CLASSIFIER_H_

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class IntRect;

class PLATFORM_EXPORT DarkModeBitmapImageClassifier {
  DISALLOW_NEW();

 public:
  DarkModeBitmapImageClassifier();
  ~DarkModeBitmapImageClassifier() = default;

  DarkModeClassification Classify(Image& image, const FloatRect& src_rect);

  bool ComputeImageFeaturesForTesting(Image& image, Vector<float>* features) {
    Vector<SkColor> sampled_pixels;
    return ComputeImageFeatures(
        image,
        FloatRect(0, 0, static_cast<float>(image.width()),
                  static_cast<float>(image.height())),
        features, &sampled_pixels);
  }

  DarkModeClassification ClassifyImageUsingDecisionTreeForTesting(
      const Vector<float>& features) {
    return ClassifyImageUsingDecisionTree(features);
  }

 private:
  enum class ColorMode { kColor = 0, kGrayscale = 1 };

  // Computes the features vector for a given image.
  bool ComputeImageFeatures(Image&,
                            const FloatRect&,
                            Vector<float>*,
                            Vector<SkColor>*);

  // Converts image to SkBitmap and returns true if successful.
  bool GetBitmap(Image&, const FloatRect&, SkBitmap*);

  // Given a SkBitmap, extracts a sample set of pixels (|sampled_pixels|),
  // |transparency_ratio|, and |background_ratio|.
  void GetSamples(const SkBitmap&,
                  Vector<SkColor>* sampled_pixels,
                  float* transparency_ratio,
                  float* background_ratio);

  // Given |sampled_pixels|, |transparency_ratio|, and |background_ratio| for an
  // image, computes the required |features| for classification.
  void GetFeatures(const Vector<SkColor>& sampled_pixels,
                   const float transparency_ratio,
                   const float background_ratio,
                   Vector<float>* features);

  // Makes a decision about the image given its features.
  DarkModeClassification ClassifyImage(const Vector<float>&);

  // Receives sampled pixels and color mode, and returns the ratio of color
  // buckets count to all possible color buckets. If image is in color, a color
  // bucket is a 4 bit per channel representation of each RGB color, and if it
  // is grayscale, each bucket is a 4 bit representation of luminance.
  float ComputeColorBucketsRatio(const Vector<SkColor>&, const ColorMode);

  // Gets the |required_samples_count| for a specific |block| of the given
  // SkBitmap, and returns |sampled_pixels| and |transparent_pixels_count|.
  void GetBlockSamples(const SkBitmap&,
                       const IntRect& block,
                       const int required_samples_count,
                       Vector<SkColor>* sampled_pixels,
                       int* transparent_pixels_count);

  // Decides if the filter should be applied to the image or not, only using the
  // decision tree. Returns 'kNotClassified' if decision tree cannot give a
  // trustable answer.
  DarkModeClassification ClassifyImageUsingDecisionTree(const Vector<float>&);

  int pixels_to_sample_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_BITMAP_IMAGE_CLASSIFIER_H_
