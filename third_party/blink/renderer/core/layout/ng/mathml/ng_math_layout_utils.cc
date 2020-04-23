// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_scripts_element.h"

namespace blink {

NGConstraintSpace CreateConstraintSpaceForMathChild(
    const NGBlockNode& parent_node,
    const LogicalSize& child_available_size,
    const NGConstraintSpace& parent_constraint_space,
    const NGLayoutInputNode& child) {
  const ComputedStyle& parent_style = parent_node.Style();
  const ComputedStyle& child_style = child.Style();
  DCHECK(child.CreatesNewFormattingContext());
  NGConstraintSpaceBuilder space_builder(parent_constraint_space,
                                         child_style.GetWritingMode(),
                                         true /* is_new_fc */);
  SetOrthogonalFallbackInlineSizeIfNeeded(parent_style, child, &space_builder);

  space_builder.SetAvailableSize(child_available_size);
  space_builder.SetPercentageResolutionSize(child_available_size);
  space_builder.SetReplacedPercentageResolutionSize(child_available_size);

  space_builder.SetIsShrinkToFit(child_style.LogicalWidth().IsAuto());

  // TODO(rbuis): add target stretch sizes.

  space_builder.SetTextDirection(child_style.Direction());

  // TODO(rbuis): add ink baselines?
  space_builder.SetNeedsBaseline(true);
  return space_builder.ToConstraintSpace();
}

NGLayoutInputNode FirstChildInFlow(const NGBlockNode& node) {
  NGLayoutInputNode child = node.FirstChild();
  while (child && child.IsOutOfFlowPositioned())
    child = child.NextSibling();
  return child;
}

NGLayoutInputNode NextSiblingInFlow(const NGBlockNode& node) {
  NGLayoutInputNode sibling = node.NextSibling();
  while (sibling && sibling.IsOutOfFlowPositioned())
    sibling = sibling.NextSibling();
  return sibling;
}

inline bool InFlowChildCountIs(const NGBlockNode& node, unsigned count) {
  DCHECK(count == 2 || count == 3);
  auto child = To<NGBlockNode>(FirstChildInFlow(node));
  while (count && child) {
    child = To<NGBlockNode>(NextSiblingInFlow(child));
    count--;
  }
  return !count && !child;
}

bool IsValidMathMLFraction(const NGBlockNode& node) {
  return InFlowChildCountIs(node, 2);
}

bool IsValidMathMLScript(const NGBlockNode& node) {
  switch (node.ScriptType()) {
    case MathScriptType::kUnder:
    case MathScriptType::kOver:
    case MathScriptType::kSub:
    case MathScriptType::kSuper:
      return InFlowChildCountIs(node, 2);
    case MathScriptType::kSubSup:
    case MathScriptType::kUnderOver:
      return InFlowChildCountIs(node, 3);
    default:
      return false;
  }
}

namespace {

inline LayoutUnit DefaultFractionLineThickness(const ComputedStyle& style) {
  return LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kFractionRuleThickness)
          .value_or(RuleThicknessFallback(style)));
}

}  // namespace

LayoutUnit MathAxisHeight(const ComputedStyle& style) {
  return LayoutUnit(
      MathConstant(style, OpenTypeMathSupport::MathConstants::kAxisHeight)
          .value_or(style.GetFont().PrimaryFont()->GetFontMetrics().XHeight() /
                    2));
}

LayoutUnit FractionLineThickness(const ComputedStyle& style) {
  return std::max<LayoutUnit>(
      ValueForLength(style.GetMathFractionBarThickness(),
                     DefaultFractionLineThickness(style)),
      LayoutUnit());
}

}  // namespace blink
