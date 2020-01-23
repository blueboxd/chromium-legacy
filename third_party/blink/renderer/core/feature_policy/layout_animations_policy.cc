// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/layout_animations_policy.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {
namespace {
String GetViolationMessage(const CSSProperty& property) {
  return String::Format(
      "Feature policy violation: CSS property '%s' violates feature policy "
      "'%s' which is disabled in this document",
      property.GetPropertyNameString().Utf8().c_str(),
      GetNameForFeature(mojom::blink::FeaturePolicyFeature::kLayoutAnimations)
          .Utf8()
          .c_str());
}
}  // namespace

LayoutAnimationsPolicy::LayoutAnimationsPolicy() = default;

// static
const HashSet<const CSSProperty*>&
LayoutAnimationsPolicy::AffectedCSSProperties() {
  DEFINE_STATIC_LOCAL(
      HashSet<const CSSProperty*>, properties,
      ({&GetCSSPropertyBottom(), &GetCSSPropertyHeight(), &GetCSSPropertyLeft(),
        &GetCSSPropertyRight(), &GetCSSPropertyTop(), &GetCSSPropertyWidth()}));
  return properties;
}

// static
void LayoutAnimationsPolicy::ReportViolation(
    const CSSProperty& animated_property,
    const ExecutionContext& context) {
  DCHECK(AffectedCSSProperties().Contains(&animated_property));
  context.IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature::kLayoutAnimations,
      ReportOptions::kReportOnFailure, GetViolationMessage(animated_property));
}

}  // namespace blink
