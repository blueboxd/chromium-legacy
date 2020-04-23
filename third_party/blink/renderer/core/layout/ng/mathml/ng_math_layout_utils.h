// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"

namespace blink {

struct LogicalSize;
class NGBlockNode;
class NGConstraintSpace;
class NGLayoutInputNode;

// Creates a new constraint space for the current child.
NGConstraintSpace CreateConstraintSpaceForMathChild(
    const NGBlockNode& parent_node,
    const LogicalSize& child_available_size,
    const NGConstraintSpace& parent_constraint_space,
    const NGLayoutInputNode&);

NGLayoutInputNode FirstChildInFlow(const NGBlockNode&);
NGLayoutInputNode NextSiblingInFlow(const NGBlockNode&);

bool IsValidMathMLFraction(const NGBlockNode&);
bool IsValidMathMLScript(const NGBlockNode&);

inline float RuleThicknessFallback(const ComputedStyle& style) {
  // This function returns a value for the default rule thickness (TeX's
  // \xi_8) to be used as a fallback when we lack a MATH table.
  return 0.05f * style.FontSize();
}

LayoutUnit MathAxisHeight(const ComputedStyle& style);

inline base::Optional<float> MathConstant(
    const ComputedStyle& style,
    OpenTypeMathSupport::MathConstants constant) {
  return OpenTypeMathSupport::MathConstant(
      style.GetFont().PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      constant);
}

LayoutUnit FractionLineThickness(const ComputedStyle& style);

inline bool HasDisplayStyle(const ComputedStyle& style) {
  return style.MathStyle() == EMathStyle::kDisplay;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_
