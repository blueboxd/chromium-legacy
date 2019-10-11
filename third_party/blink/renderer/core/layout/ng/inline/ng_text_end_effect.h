// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_END_EFFECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_END_EFFECT_H_

namespace blink {

// Effects at the end of text fragments.
enum class NGTextEndEffect {
  kNone,
  kHyphen,

  // When adding new values, ensure NGPhysicalTextFragment has enough bits.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_END_EFFECT_H_
