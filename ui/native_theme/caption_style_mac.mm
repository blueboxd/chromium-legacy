// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <AppKit/AppKit.h>
#include <MediaAccessibility/MediaAccessibility.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/caption_style.h"

namespace ui {
// static
absl::optional<CaptionStyle> CaptionStyle::FromSystemSettings() {
  if (!base::FeatureList::IsEnabled(features::kSystemCaptionStyle))
    return absl::nullopt;

  CaptionStyle style;

  return style;
}

}  // namespace ui
