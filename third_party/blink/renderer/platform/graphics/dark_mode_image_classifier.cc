// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"

namespace blink {
namespace {

bool IsColorGray(const SkColor& color) {
  return abs(static_cast<int>(SkColorGetR(color)) -
             static_cast<int>(SkColorGetG(color))) +
             abs(static_cast<int>(SkColorGetG(color)) -
                 static_cast<int>(SkColorGetB(color))) <=
         8;
}

bool IsColorTransparent(const SkColor& color) {
  return (SkColorGetA(color) < 128);
}

const int kPixelsToSample = 1000;
const int kBlocksCount1D = 10;
const float kMinOpaquePixelPercentageForForeground = 0.2;

}  // namespace

DarkModeImageClassifier::DarkModeImageClassifier()
    : pixels_to_sample_(kPixelsToSample) {}

DarkModeClassification DarkModeImageClassifier::Classify(
    Image* image,
    const FloatRect& src_rect) {
  DarkModeClassification result = image->GetDarkModeClassification(src_rect);
  if (result != DarkModeClassification::kNotClassified)
    return result;

  result = image->CheckTypeSpecificConditionsForDarkMode(src_rect, this);
  if (result != DarkModeClassification::kNotClassified) {
    image->AddDarkModeClassification(src_rect, result);
    return result;
  }

  Vector<float> features;
  if (!GetFeatures(image, src_rect, &features)) {
    // Do not cache this classification.
    return DarkModeClassification::kDoNotApplyFilter;
  }

  result = ClassifyWithFeatures(features);
  image->AddDarkModeClassification(src_rect, result);
  return result;
}

bool DarkModeImageClassifier::GetFeatures(Image* image,
                                          const FloatRect& src_rect,
                                          Vector<float>* features) {
  SkBitmap bitmap;
  if (!image->GetImageBitmap(src_rect, &bitmap))
    return false;

  if (pixels_to_sample_ > src_rect.Width() * src_rect.Height())
    pixels_to_sample_ = src_rect.Width() * src_rect.Height();

  float transparency_ratio;
  float background_ratio;
  Vector<SkColor> sampled_pixels;
  GetSamples(bitmap, &sampled_pixels, &transparency_ratio, &background_ratio);
  // TODO(https://crbug.com/945434): Investigate why an incorrect resource is
  // loaded and how we can fetch the correct resource. This condition will
  // prevent going further with the rest of the classification logic.
  if (sampled_pixels.size() == 0)
    return false;

  ComputeFeatures(sampled_pixels, transparency_ratio, background_ratio,
                  features);
  return true;
}

// Extracts sample pixels from the image. The image is separated into uniformly
// distributed blocks through its width and height, each block is sampled, and
// checked to see if it seems to be background or foreground.
void DarkModeImageClassifier::GetSamples(const SkBitmap& bitmap,
                                         Vector<SkColor>* sampled_pixels,
                                         float* transparency_ratio,
                                         float* background_ratio) {
  int pixels_per_block = pixels_to_sample_ / (kBlocksCount1D * kBlocksCount1D);

  int transparent_pixels = 0;
  int opaque_pixels = 0;
  int blocks_count = 0;

  Vector<int> horizontal_grid(kBlocksCount1D + 1);
  Vector<int> vertical_grid(kBlocksCount1D + 1);
  for (int block = 0; block <= kBlocksCount1D; block++) {
    horizontal_grid[block] = static_cast<int>(
        round(block * bitmap.width() / static_cast<float>(kBlocksCount1D)));
    vertical_grid[block] = static_cast<int>(
        round(block * bitmap.height() / static_cast<float>(kBlocksCount1D)));
  }

  sampled_pixels->clear();
  Vector<IntRect> foreground_blocks;

  for (int y = 0; y < kBlocksCount1D; y++) {
    for (int x = 0; x < kBlocksCount1D; x++) {
      IntRect block(horizontal_grid[x], vertical_grid[y],
                    horizontal_grid[x + 1] - horizontal_grid[x],
                    vertical_grid[y + 1] - vertical_grid[y]);

      Vector<SkColor> block_samples;
      int block_transparent_pixels;
      GetBlockSamples(bitmap, block, pixels_per_block, &block_samples,
                      &block_transparent_pixels);
      opaque_pixels += static_cast<int>(block_samples.size());
      transparent_pixels += block_transparent_pixels;
      sampled_pixels->AppendRange(block_samples.begin(), block_samples.end());
      if (opaque_pixels >
          kMinOpaquePixelPercentageForForeground * pixels_per_block) {
        foreground_blocks.push_back(block);
      }
      blocks_count++;
    }
  }

  *transparency_ratio = static_cast<float>(transparent_pixels) /
                        (transparent_pixels + opaque_pixels);
  *background_ratio =
      1.0 - static_cast<float>(foreground_blocks.size()) / blocks_count;
}

// Selects samples at regular intervals from a block of the image.
// Returns the opaque sampled pixels, and the number of transparent
// sampled pixels.
void DarkModeImageClassifier::GetBlockSamples(const SkBitmap& bitmap,
                                              const IntRect& block,
                                              const int required_samples_count,
                                              Vector<SkColor>* sampled_pixels,
                                              int* transparent_pixels_count) {
  *transparent_pixels_count = 0;

  int x1 = block.X();
  int y1 = block.Y();
  int x2 = block.MaxX();
  int y2 = block.MaxY();
  DCHECK(x1 < bitmap.width());
  DCHECK(y1 < bitmap.height());
  DCHECK(x2 <= bitmap.width());
  DCHECK(y2 <= bitmap.height());

  sampled_pixels->clear();

  int cx = static_cast<int>(
      ceil(static_cast<float>(x2 - x1) / sqrt(required_samples_count)));
  int cy = static_cast<int>(
      ceil(static_cast<float>(y2 - y1) / sqrt(required_samples_count)));

  for (int y = y1; y < y2; y += cy) {
    for (int x = x1; x < x2; x += cx) {
      SkColor new_sample = bitmap.getColor(x, y);
      if (IsColorTransparent(new_sample))
        (*transparent_pixels_count)++;
      else
        sampled_pixels->push_back(new_sample);
    }
  }
}

// This function computes a single feature vector from a sample set of image
// pixels. The current features are:
// 0: 1 if color, 0 if grayscale.
// 1: Ratio of the number of bucketed colors used in the image to all
//    possibilities. Color buckets are represented with 4 bits per color
//    channel.
// 2: Ratio of transparent area to the whole image.
// 3: Ratio of the background area to the whole image.
void DarkModeImageClassifier::ComputeFeatures(
    const Vector<SkColor>& sampled_pixels,
    const float transparency_ratio,
    const float background_ratio,
    Vector<float>* features) {
  int samples_count = static_cast<int>(sampled_pixels.size());

  // Is image grayscale.
  int color_pixels = 0;
  for (const SkColor& sample : sampled_pixels) {
    if (!IsColorGray(sample))
      color_pixels++;
  }
  ColorMode color_mode = (color_pixels > samples_count / 100)
                             ? ColorMode::kColor
                             : ColorMode::kGrayscale;

  features->resize(5);

  // Feature 0: Is Colorful?
  (*features)[0] = color_mode == ColorMode::kColor;

  // Feature 1: Color Buckets Ratio.
  (*features)[1] = ComputeColorBucketsRatio(sampled_pixels, color_mode);

  // Feature 2: Transparency Ratio
  (*features)[2] = transparency_ratio;

  // Feature 3: Background Ratio.
  (*features)[3] = background_ratio;

  // Feature 4: Is SVG?
  (*features)[4] = image_type_ == ImageType::kSvg;
}

float DarkModeImageClassifier::ComputeColorBucketsRatio(
    const Vector<SkColor>& sampled_pixels,
    const ColorMode color_mode) {
  HashSet<unsigned, WTF::AlreadyHashed,
          WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
      buckets;

  // If image is in color, use 4 bits per color channel, otherwise 4 bits for
  // illumination.
  if (color_mode == ColorMode::kColor) {
    for (const SkColor& sample : sampled_pixels) {
      unsigned bucket = ((SkColorGetR(sample) >> 4) << 8) +
                        ((SkColorGetG(sample) >> 4) << 4) +
                        ((SkColorGetB(sample) >> 4));
      buckets.insert(bucket);
    }
  } else {
    for (const SkColor& sample : sampled_pixels) {
      unsigned illumination =
          (SkColorGetR(sample) * 5 + SkColorGetG(sample) * 3 +
           SkColorGetB(sample) * 2) /
          10;
      buckets.insert(illumination / 16);
    }
  }

  // Using 4 bit per channel representation of each color bucket, there would be
  // 2^4 buckets for grayscale images and 2^12 for color images.
  const float max_buckets[] = {16, 4096};
  return static_cast<float>(buckets.size()) /
         max_buckets[color_mode == ColorMode::kColor];
}

}  // namespace blink
