// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_MAC_CORE_TEXT_FONT_FORMAT_SUPPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_MAC_CORE_TEXT_FONT_FORMAT_SUPPORT_H_

namespace blink {

bool CoreTextVersionSupportsVariations();
bool CoreTextVersionSupportsColrCpal();
// See https://bugs.chromium.org/p/skia/issues/detail?id=9747 - Depending on
// variation axes parameters Mac OS pre 10.15 produces broken SkTypefaces when
// using makeClone() on system fonts.
bool CoreTextVersionSupportsSystemFontMakeClone();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_MAC_CORE_TEXT_FONT_FORMAT_SUPPORT_H_
