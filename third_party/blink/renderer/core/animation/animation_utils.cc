// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_utils.h"

#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void AnimationUtils::ForEachInterpolatedPropertyValue(
    Element* target,
    const PropertyHandleSet& properties,
    ActiveInterpolationsMap& interpolations,
    base::RepeatingCallback<void(PropertyHandle, const CSSValue*)> callback) {
  if (!target)
    return;

  StyleResolver& resolver = target->GetDocument().EnsureStyleResolver();
  scoped_refptr<ComputedStyle> style =
      resolver.StyleForInterpolations(*target, interpolations);

  for (const auto& property : properties) {
    if (!property.IsCSSProperty())
      continue;

    // TODO(crbug.com/1057307): Fix to use actual computed style.
    CSSPropertyRef ref(property.GetCSSPropertyName(), target->GetDocument());
    const CSSValue* value = ref.GetProperty().CSSValueFromComputedStyle(
        *style, target->GetLayoutObject(), false);
    if (!value)
      continue;

    callback.Run(property, value);
  }
}

}  // namespace blink
