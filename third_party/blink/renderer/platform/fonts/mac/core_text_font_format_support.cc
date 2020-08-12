// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/core_text_font_format_support.h"

#include "base/mac/mac_util.h"

namespace blink {

bool CoreTextVersionSupportsVariations() {
  return base::mac::IsAtLeastOS10_14();
}

// CoreText versions below 10.13 display COLR cpal as black/foreground-color
// glyphs and do not interpret color glyph layers correctly.
bool CoreTextVersionSupportsColrCpal() {
  return base::mac::IsAtLeastOS10_13();
}

}  // namespace blink
