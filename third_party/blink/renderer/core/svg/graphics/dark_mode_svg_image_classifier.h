// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_DARK_MODE_SVG_IMAGE_CLASSIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_DARK_MODE_SVG_IMAGE_CLASSIFIER_H_

#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT DarkModeSVGImageClassifier {
  DISALLOW_NEW();

 public:
  DarkModeSVGImageClassifier();
  ~DarkModeSVGImageClassifier() = default;

  DarkModeClassification Classify(SVGImage* image, const FloatRect& src_rect);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_DARK_MODE_SVG_IMAGE_CLASSIFIER_H_
