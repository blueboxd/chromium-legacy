// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CLASSIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CLASSIFIER_H_

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT DarkModeImageClassifier {
  DISALLOW_NEW();

 public:
  DarkModeImageClassifier();
  ~DarkModeImageClassifier() = default;

  bool GetFeaturesForTesting(Image* image, Vector<float>* features) {
    Vector<SkColor> sampled_pixels;
    return GetFeatures(image,
                       FloatRect(0, 0, static_cast<float>(image->width()),
                                 static_cast<float>(image->height())),
                       features);
  }

  DarkModeClassification Classify(Image* image, const FloatRect& src_rect);

  // Computes the features vector for a given image.
  bool GetFeatures(Image* image,
                   const FloatRect& src_rect,
                   Vector<float>* features);

  virtual DarkModeClassification ClassifyWithFeatures(
      const Vector<float> features) {
    return DarkModeClassification::kDoNotApplyFilter;
  }

  enum class ImageType { kBitmap = 0, kSvg = 1 };

  void SetImageType(ImageType image_type) { image_type_ = image_type; }

 private:
  enum class ColorMode { kColor = 0, kGrayscale = 1 };

  // Given a SkBitmap, extracts a sample set of pixels (|sampled_pixels|),
  // |transparency_ratio|, and |background_ratio|.
  void GetSamples(const SkBitmap& bitmap,
                  Vector<SkColor>* sampled_pixels,
                  float* transparency_ratio,
                  float* background_ratio);

  // Gets the |required_samples_count| for a specific |block| of the given
  // SkBitmap, and returns |sampled_pixels| and |transparent_pixels_count|.
  void GetBlockSamples(const SkBitmap& bitmap,
                       const IntRect& block,
                       const int required_samples_count,
                       Vector<SkColor>* sampled_pixels,
                       int* transparent_pixels_count);

  // Given |sampled_pixels|, |transparency_ratio|, and |background_ratio| for an
  // image, computes the required |features| for classification.
  void ComputeFeatures(const Vector<SkColor>& sampled_pixels,
                       const float transparency_ratio,
                       const float background_ratio,
                       Vector<float>* features);

  // Receives sampled pixels and color mode, and returns the ratio of color
  // buckets count to all possible color buckets. If image is in color, a color
  // bucket is a 4 bit per channel representation of each RGB color, and if it
  // is grayscale, each bucket is a 4 bit representation of luminance.
  float ComputeColorBucketsRatio(const Vector<SkColor>& sampled_pixels,
                                 const ColorMode color_mode);

  int pixels_to_sample_;

  ImageType image_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_IMAGE_CLASSIFIER_H_
