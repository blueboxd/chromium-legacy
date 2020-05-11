// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_pending_image.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

CSSValue* StylePendingImage::ComputedCSSValue(const ComputedStyle& style,
                                              bool allow_visited_style) const {
  DCHECK(style.IsEnsuredInDisplayNone() ||
         style.Display() == EDisplay::kContents);

  if (CSSImageValue* image_value = CssImageValue())
    return image_value->ValueWithURLMadeAbsolute();
  if (CSSImageSetValue* image_set_value = CssImageSetValue())
    return image_set_value->ValueWithURLsMadeAbsolute();
  if (CSSImageGeneratorValue* image_generator_value = CssImageGeneratorValue())
    return image_generator_value->ComputedCSSValue(style, allow_visited_style);
  NOTREACHED();
  return CssValue();
}

}  // namespace blink
