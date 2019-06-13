// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_

#include <memory>

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorFilter;

namespace blink {

class DarkModeColorClassifier;
class DarkModeColorFilter;

class PLATFORM_EXPORT DarkModeFilter {
 public:
  // Dark mode is disabled by default. Enable it by calling UpdateSettings()
  // with a mode other than DarkMode::kOff.
  DarkModeFilter();
  ~DarkModeFilter();

  const DarkModeSettings& settings() const { return settings_; }
  void UpdateSettings(const DarkModeSettings& new_settings);

  Color InvertColorIfNeeded(const Color& color);

  // |image| and |flags| must not be null.
  void ApplyToImageFlagsIfNeeded(const FloatRect& src_rect,
                                 Image* image,
                                 cc::PaintFlags* flags);

  base::Optional<cc::PaintFlags> ApplyToFlagsIfNeeded(
      const cc::PaintFlags& flags);

  bool ShouldInvertTextColor(const Color& color) const;

  bool IsDarkModeActive() const;

 private:
  DarkModeSettings settings_;

  std::unique_ptr<DarkModeColorClassifier> text_classifier_;

  std::unique_ptr<DarkModeColorFilter> color_filter_;
  sk_sp<SkColorFilter> image_filter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
