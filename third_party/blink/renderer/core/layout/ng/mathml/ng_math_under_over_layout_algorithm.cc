// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_under_over_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"

namespace blink {
namespace {

// Describes the amount to shift to apply to the under/over boxes.
// Data is populated from the OpenType MATH table.
// If the OpenType MATH table is not present fallback values are used.
// https://mathml-refresh.github.io/mathml-core/#base-with-underscript
// https://mathml-refresh.github.io/mathml-core/#base-with-overscript
struct UnderOverVerticalParameters {
  bool use_under_over_bar_fallback;
  LayoutUnit under_gap_min;
  LayoutUnit over_gap_min;
  LayoutUnit under_shift_min;
  LayoutUnit over_shift_min;
  LayoutUnit under_extra_descender;
  LayoutUnit over_extra_ascender;
  LayoutUnit accent_base_height;
};

UnderOverVerticalParameters GetUnderOverVerticalParameters(
    const ComputedStyle& style) {
  UnderOverVerticalParameters parameters;

  if (!OpenTypeMathSupport::HasMathData(
          style.GetFont().PrimaryFont()->PlatformData().GetHarfBuzzFace())) {
    // The MATH table specification does not really provide any suggestions,
    // except for some underbar/overbar values and AccentBaseHeight.
    LayoutUnit default_line_thickness =
        LayoutUnit(RuleThicknessFallback(style));
    parameters.under_gap_min = 3 * default_line_thickness;
    parameters.over_gap_min = 3 * default_line_thickness;
    parameters.under_extra_descender = default_line_thickness;
    parameters.over_extra_ascender = default_line_thickness;
    parameters.accent_base_height =
        LayoutUnit(style.GetFont().PrimaryFont()->GetFontMetrics().XHeight());
    parameters.use_under_over_bar_fallback = true;
    return parameters;
  }

  // The base is a large operator so we read UpperLimit/LowerLimit constants
  // from the MATH table.
  parameters.under_gap_min = LayoutUnit(
      MathConstant(style, OpenTypeMathSupport::MathConstants::kLowerLimitGapMin)
          .value_or(0));
  parameters.over_gap_min = LayoutUnit(
      MathConstant(style, OpenTypeMathSupport::MathConstants::kUpperLimitGapMin)
          .value_or(0));
  parameters.under_shift_min = LayoutUnit(
      MathConstant(
          style, OpenTypeMathSupport::MathConstants::kLowerLimitBaselineDropMin)
          .value_or(0));
  parameters.over_shift_min = LayoutUnit(
      MathConstant(
          style, OpenTypeMathSupport::MathConstants::kUpperLimitBaselineRiseMin)
          .value_or(0));
  parameters.use_under_over_bar_fallback = false;
  return parameters;
}

}  // namespace

NGMathUnderOverLayoutAlgorithm::NGMathUnderOverLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_scrollbar_padding_(params.fragment_geometry.border +
                                params.fragment_geometry.padding +
                                params.fragment_geometry.scrollbar) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

void NGMathUnderOverLayoutAlgorithm::GatherChildren(NGBlockNode* base,
                                                    NGBlockNode* over,
                                                    NGBlockNode* under) {
  auto script_type = Node().ScriptType();
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    NGBlockNode block_child = To<NGBlockNode>(child);
    if (child.IsOutOfFlowPositioned()) {
      container_builder_.AddOutOfFlowChildCandidate(
          block_child, {border_scrollbar_padding_.inline_start,
                        border_scrollbar_padding_.block_start});
      continue;
    }
    if (!*base) {
      *base = block_child;
      continue;
    }
    switch (script_type) {
      case MathScriptType::kUnder:
        DCHECK(!*under);
        *under = block_child;
        break;
      case MathScriptType::kOver:
        DCHECK(!*over);
        *over = block_child;
        break;
      case MathScriptType::kUnderOver:
        if (!*under) {
          *under = block_child;
          continue;
        }
        DCHECK(!*over);
        *over = block_child;
        break;
      default:
        NOTREACHED();
    }
  }
}

scoped_refptr<const NGLayoutResult> NGMathUnderOverLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());
  DCHECK(IsValidMathMLScript(Node()));

  NGBlockNode base = nullptr;
  NGBlockNode over = nullptr;
  NGBlockNode under = nullptr;
  GatherChildren(&base, &over, &under);

  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  auto child_available_size =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding_);

  LayoutUnit block_offset = border_scrollbar_padding_.block_start;
  UnderOverVerticalParameters parameters =
      GetUnderOverVerticalParameters(Style());
  // TODO(rbuis): handle stretchy operators.
  // TODO(rbuis): handle accent.

  // All children are positioned centered relative to the container (and
  // therefore centered relative to themselves).
  if (over) {
    auto over_space = CreateConstraintSpaceForMathChild(
        Node(), child_available_size, ConstraintSpace(), over);
    scoped_refptr<const NGLayoutResult> over_layout_result =
        over.Layout(over_space);
    NGBoxStrut over_margins =
        ComputeMarginsFor(over_space, over.Style(), ConstraintSpace());
    NGBoxFragment over_fragment(
        ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
        To<NGPhysicalBoxFragment>(over_layout_result->PhysicalFragment()));
    block_offset += parameters.over_extra_ascender + over_margins.block_start;
    LogicalOffset over_offset = {
        border_scrollbar_padding_.inline_start + over_margins.inline_start +
            (child_available_size.inline_size -
             (over_fragment.InlineSize() + over_margins.InlineSum())) /
                2,
        block_offset};
    container_builder_.AddChild(over_layout_result->PhysicalFragment(),
                                over_offset);
    over.StoreMargins(ConstraintSpace(), over_margins);
    if (parameters.use_under_over_bar_fallback) {
      block_offset += over_fragment.BlockSize();
      block_offset += parameters.over_gap_min;
    } else {
      LayoutUnit over_ascent =
          over_fragment.Baseline().value_or(over_fragment.BlockSize());
      block_offset +=
          std::max(over_fragment.BlockSize() + parameters.over_gap_min,
                   over_ascent + parameters.over_shift_min);
    }
    block_offset += over_margins.block_end;
  }

  auto base_space = CreateConstraintSpaceForMathChild(
      Node(), child_available_size, ConstraintSpace(), base);
  auto base_layout_result = base.Layout(base_space);
  auto base_margins =
      ComputeMarginsFor(base_space, base.Style(), ConstraintSpace());

  NGBoxFragment base_fragment(
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
      To<NGPhysicalBoxFragment>(base_layout_result->PhysicalFragment()));

  block_offset += base_margins.block_start;
  LogicalOffset base_offset = {
      border_scrollbar_padding_.inline_start + base_margins.inline_start +
          (child_available_size.inline_size -
           (base_fragment.InlineSize() + base_margins.InlineSum())) /
              2,
      block_offset};
  container_builder_.AddChild(base_layout_result->PhysicalFragment(),
                              base_offset);
  base.StoreMargins(ConstraintSpace(), base_margins);
  block_offset += base_fragment.BlockSize() + base_margins.block_end;

  if (under) {
    auto under_space = CreateConstraintSpaceForMathChild(
        Node(), child_available_size, ConstraintSpace(), under);
    scoped_refptr<const NGLayoutResult> under_layout_result =
        under.Layout(under_space);
    NGBoxStrut under_margins =
        ComputeMarginsFor(under_space, under.Style(), ConstraintSpace());
    NGBoxFragment under_fragment(
        ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
        To<NGPhysicalBoxFragment>(under_layout_result->PhysicalFragment()));
    block_offset += under_margins.block_start;
    if (parameters.use_under_over_bar_fallback) {
      block_offset += parameters.under_gap_min;
    } else {
      LayoutUnit under_ascent =
          under_fragment.Baseline().value_or(under_fragment.BlockSize());
      block_offset += std::max(parameters.under_gap_min,
                               parameters.under_shift_min - under_ascent);
    }
    LogicalOffset under_offset = {
        border_scrollbar_padding_.inline_start + under_margins.inline_start +
            (child_available_size.inline_size -
             (under_fragment.InlineSize() + under_margins.InlineSum())) /
                2,
        block_offset};
    block_offset += under_fragment.BlockSize();
    block_offset += parameters.under_extra_descender;
    container_builder_.AddChild(under_layout_result->PhysicalFragment(),
                                under_offset);
    under.StoreMargins(ConstraintSpace(), under_margins);
    block_offset += under_margins.block_end;
  }

  LayoutUnit base_ascent =
      base_fragment.Baseline().value_or(base_fragment.BlockSize());
  container_builder_.SetBaseline(base_offset.block_offset + base_ascent);

  block_offset += border_scrollbar_padding_.block_end;

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_scrollbar_padding_, block_offset,
      border_box_size.inline_size);

  container_builder_.SetIntrinsicBlockSize(block_offset);
  container_builder_.SetBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), container_builder_.Borders(),
                        &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizes NGMathUnderOverLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  DCHECK(IsValidMathMLScript(Node()));

  if (auto sizes = CalculateMinMaxSizesIgnoringChildren(
          Node(), border_scrollbar_padding_))
    return *sizes;

  MinMaxSizes sizes;
  LayoutUnit child_percentage_resolution_block_size =
      CalculateChildPercentageBlockSizeForMinMax(
          ConstraintSpace(), Node(), border_scrollbar_padding_,
          input.percentage_resolution_block_size);

  MinMaxSizesInput child_input(child_percentage_resolution_block_size);

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    auto child_sizes =
        ComputeMinAndMaxContentContribution(Style(), child, child_input);
    NGBoxStrut margins = ComputeMinMaxMargins(Style(), child);
    child_sizes += margins.InlineSum();
    sizes.Encompass(child_sizes);
  }

  sizes += border_scrollbar_padding_.InlineSum();
  return sizes;
}

}  // namespace blink
