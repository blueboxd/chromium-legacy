// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_keyframe_shorthand_value.h"

namespace blink {

CSSKeyframeShorthandValue::CSSKeyframeShorthandValue(
    ImmutableCSSPropertyValueSet* properties)
    : CSSValue(kKeyframeShorthandClass), properties_(properties) {}

String CSSKeyframeShorthandValue::CustomCSSText() const {
  // All property/value pairs belong to the same shorthand so we grab the id
  // from the first one.
  CSSPropertyID my_shorthand = properties_->PropertyAt(0).ShorthandID();
#if DCHECK_IS_ON()
  for (unsigned i = 0; i < properties_->PropertyCount(); i++) {
    DCHECK_EQ(my_shorthand, properties_->PropertyAt(i).ShorthandID())
        << "These are not the longhands you're looking for.";
  }
#endif

  return properties_->GetPropertyValue(my_shorthand);
}

void CSSKeyframeShorthandValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(properties_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
