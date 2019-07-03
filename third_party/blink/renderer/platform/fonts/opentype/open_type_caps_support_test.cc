// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_caps_support.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"

#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace blink {

void ensureHasNativeSmallCaps(const char* font_family_name) {
  sk_sp<SkTypeface> test_typeface =
      SkTypeface::MakeFromName(font_family_name, SkFontStyle());
  FontPlatformData font_platform_data(test_typeface, font_family_name, 16,
                                      false, false);
  ASSERT_EQ(font_platform_data.FontFamilyName(), font_family_name);

  OpenTypeCapsSupport caps_support(font_platform_data.GetHarfBuzzFace(),
                                   FontDescription::FontVariantCaps::kSmallCaps,
                                   HB_SCRIPT_LATIN);
  // If caps_support.NeedsRunCaseSplitting() is true, this means that synthetic
  // upper-casing / lower-casing is required and the run needs to be segmented
  // by upper-case, lower-case properties. If it is false, it means that the
  // font feature can be used and no synthetic case-changing is needed.
  ASSERT_FALSE(caps_support.NeedsRunCaseSplitting());
}

// The AAT fonts for testing are only available on Mac OS X 10.13.
#if defined(OS_MACOSX)
#define MAYBE_SmallCapsForSFNSText SmallCapsForSFNSText
#else
#define MAYBE_SmallCapsForSFNSText DISABLED_SmallCapsForSFNSText
#endif
TEST(OpenTypeCapsSupportTest, MAYBE_SmallCapsForSFNSText) {
#if defined(OS_MACOSX)
  if (!base::mac::IsAtLeastOS10_13())
    return;
#endif
  Vector<const char*> test_fonts = {
      ".SF NS Text",     // has OpenType small-caps
      "Apple Chancery",  // has old-style (feature id 3,"Letter Case")
                         // small-caps
      "Baskerville"};    // has new-style (feature id 38, "Upper Case")
                         // small-case.
  for (auto* test_font : test_fonts)
    ensureHasNativeSmallCaps(test_font);
}

}  // namespace blink
