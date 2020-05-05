// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_fraction_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"

namespace blink {
namespace {

// Describes the amount to shift the numerator/denominator of the fraction when
// a fraction bar is present. Data is populated from the OpenType MATH table.
// If the OpenType MATH table is not present fallback values are used.
// https://mathml-refresh.github.io/mathml-core/#fraction-with-nonzero-line-thickness
struct FractionParameters {
  LayoutUnit numerator_gap_min;
  LayoutUnit denominator_gap_min;
  LayoutUnit numerator_min_shift_up;
  LayoutUnit denominator_min_shift_down;
};

FractionParameters GetFractionParameters(const ComputedStyle& style) {
  FractionParameters parameters;

  bool has_display_style = HasDisplayStyle(style);

  // We try and read constants to draw the fraction from the OpenType MATH and
  // use fallback values otherwise.
  // The MATH table specification suggests default rule thickness or (in
  // displaystyle) 3 times default rule thickness for the gaps.
  parameters.numerator_gap_min = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kFractionNumDisplayStyleGapMin
              : OpenTypeMathSupport::MathConstants::kFractionNumeratorGapMin)
          .value_or((has_display_style ? 3 : 1) *
                    RuleThicknessFallback(style)));
  parameters.denominator_gap_min = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kFractionDenomDisplayStyleGapMin
              : OpenTypeMathSupport::MathConstants::kFractionDenominatorGapMin)
          .value_or(parameters.numerator_gap_min));

  // TODO(crbug.com/1058369): The MATH table specification does not suggest
  // any values for shifts, so we leave them at zero for now.
  parameters.numerator_min_shift_up = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kFractionNumeratorDisplayStyleShiftUp
              : OpenTypeMathSupport::MathConstants::kFractionNumeratorShiftUp)
          .value_or(0));
  parameters.denominator_min_shift_down = LayoutUnit(
      MathConstant(style, has_display_style
                              ? OpenTypeMathSupport::MathConstants::
                                    kFractionDenominatorDisplayStyleShiftDown
                              : OpenTypeMathSupport::MathConstants::
                                    kFractionDenominatorShiftDown)
          .value_or(0));

  return parameters;
}

// Describes the amount to shift the numerator/denominator of the fraction when
// a fraction bar is not present. Data is populated from the OpenType MATH
// table. If the OpenType MATH table is not present fallback values are used.
// https://mathml-refresh.github.io/mathml-core/#fraction-with-zero-line-thickness
struct FractionStackParameters {
  LayoutUnit gap_min;
  LayoutUnit top_shift_up;
  LayoutUnit bottom_shift_down;
};

FractionStackParameters GetFractionStackParameters(const ComputedStyle& style) {
  FractionStackParameters parameters;

  bool has_display_style = HasDisplayStyle(style);

  // We try and read constants to draw the stack from the OpenType MATH and use
  // fallback values otherwise.
  // We use the fallback values suggested in the MATH table specification.
  parameters.gap_min = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::kStackDisplayStyleGapMin
              : OpenTypeMathSupport::MathConstants::kStackGapMin)
          .value_or((has_display_style ? 7 : 3) *
                    RuleThicknessFallback(style)));
  // The MATH table specification does not suggest any values for shifts, so
  // we leave them at zero.
  parameters.top_shift_up = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::kStackTopDisplayStyleShiftUp
              : OpenTypeMathSupport::MathConstants::kStackTopShiftUp)
          .value_or(0));
  parameters.bottom_shift_down = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kStackBottomDisplayStyleShiftDown
              : OpenTypeMathSupport::MathConstants::kStackBottomShiftDown)
          .value_or(0));

  return parameters;
}

}  // namespace

NGMathFractionLayoutAlgorithm::NGMathFractionLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_scrollbar_padding_(params.fragment_geometry.border +
                                params.fragment_geometry.padding +
                                params.fragment_geometry.scrollbar) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
  container_builder_.SetIsMathMLFraction();
}

void NGMathFractionLayoutAlgorithm::GatherChildren(NGBlockNode* numerator,
                                                   NGBlockNode* denominator) {
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    NGBlockNode block_child = To<NGBlockNode>(child);
    if (child.IsOutOfFlowPositioned()) {
      container_builder_.AddOutOfFlowChildCandidate(
          block_child, {border_scrollbar_padding_.inline_start,
                        border_scrollbar_padding_.block_start});
      continue;
    }
    if (!*numerator) {
      *numerator = block_child;
      continue;
    }
    if (!*denominator) {
      *denominator = block_child;
      continue;
    }

    NOTREACHED();
  }

  DCHECK(*numerator);
  DCHECK(*denominator);
}

scoped_refptr<const NGLayoutResult> NGMathFractionLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  NGBlockNode numerator = nullptr;
  NGBlockNode denominator = nullptr;
  GatherChildren(&numerator, &denominator);

  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();
  auto child_available_size =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding_);
  auto numerator_space = CreateConstraintSpaceForMathChild(
      Node(), child_available_size, ConstraintSpace(), numerator);
  scoped_refptr<const NGLayoutResult> numerator_layout_result =
      numerator.Layout(numerator_space);
  auto numerator_margins =
      ComputeMarginsFor(numerator_space, numerator.Style(), ConstraintSpace());
  auto denominator_space = CreateConstraintSpaceForMathChild(
      Node(), child_available_size, ConstraintSpace(), denominator);
  scoped_refptr<const NGLayoutResult> denominator_layout_result =
      denominator.Layout(denominator_space);
  auto denominator_margins = ComputeMarginsFor(
      denominator_space, denominator.Style(), ConstraintSpace());

  NGBoxFragment numerator_fragment(
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
      To<NGPhysicalBoxFragment>(numerator_layout_result->PhysicalFragment()));
  NGBoxFragment denominator_fragment(
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
      To<NGPhysicalBoxFragment>(denominator_layout_result->PhysicalFragment()));

  LayoutUnit numerator_ascent =
      numerator_margins.block_start +
      numerator_fragment.Baseline().value_or(numerator_fragment.BlockSize());
  LayoutUnit numerator_descent = numerator_fragment.BlockSize() +
                                 numerator_margins.BlockSum() -
                                 numerator_ascent;
  LayoutUnit denominator_ascent = denominator_margins.block_start +
                                  denominator_fragment.Baseline().value_or(
                                      denominator_fragment.BlockSize());
  LayoutUnit denominator_descent = denominator_fragment.BlockSize() +
                                   denominator_margins.BlockSum() -
                                   denominator_ascent;

  LayoutUnit numerator_shift, denominator_shift;
  LayoutUnit thickness = FractionLineThickness(Style());
  if (thickness) {
    LayoutUnit axis_height = MathAxisHeight(Style());
    FractionParameters parameters = GetFractionParameters(Style());
    numerator_shift =
        std::max(parameters.numerator_min_shift_up,
                 axis_height + thickness / 2 + parameters.numerator_gap_min +
                     numerator_descent);
    denominator_shift =
        std::max(parameters.denominator_min_shift_down,
                 thickness / 2 + parameters.denominator_gap_min +
                     denominator_ascent - axis_height);
  } else {
    FractionStackParameters parameters = GetFractionStackParameters(Style());
    numerator_shift = parameters.top_shift_up;
    denominator_shift = parameters.bottom_shift_down;
    LayoutUnit gap = denominator_shift - denominator_ascent + numerator_shift -
                     numerator_descent;
    if (gap < parameters.gap_min) {
      LayoutUnit diff = parameters.gap_min - gap;
      LayoutUnit delta = diff / 2;
      numerator_shift += delta;
      denominator_shift += diff - delta;
    }
  }

  LayoutUnit fraction_ascent =
      std::max(numerator_shift + numerator_ascent,
               -denominator_shift + denominator_ascent);
  LayoutUnit fraction_descent =
      std::max(-numerator_shift + numerator_descent,
               denominator_shift + denominator_descent);
  fraction_ascent += border_scrollbar_padding_.block_start;
  fraction_descent += border_scrollbar_padding_.block_end;
  LayoutUnit total_block_size = fraction_ascent + fraction_descent;

  container_builder_.SetBaseline(fraction_ascent);

  LogicalOffset numerator_offset;
  LogicalOffset denominator_offset;
  numerator_offset.inline_offset =
      border_scrollbar_padding_.inline_start + numerator_margins.inline_start +
      (child_available_size.inline_size -
       (numerator_fragment.InlineSize() + numerator_margins.InlineSum())) /
          2;
  denominator_offset.inline_offset =
      border_scrollbar_padding_.inline_start +
      denominator_margins.inline_start +
      (child_available_size.inline_size -
       (denominator_fragment.InlineSize() + denominator_margins.InlineSum())) /
          2;

  numerator_offset.block_offset = numerator_margins.block_start +
                                  fraction_ascent - numerator_shift -
                                  numerator_ascent;
  denominator_offset.block_offset = denominator_margins.block_start +
                                    fraction_ascent + denominator_shift -
                                    denominator_ascent;

  container_builder_.AddChild(numerator_layout_result->PhysicalFragment(),
                              numerator_offset);
  container_builder_.AddChild(denominator_layout_result->PhysicalFragment(),
                              denominator_offset);

  numerator.StoreMargins(ConstraintSpace(), numerator_margins);
  denominator.StoreMargins(ConstraintSpace(), denominator_margins);

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_scrollbar_padding_, total_block_size,
      border_box_size.inline_size);

  container_builder_.SetIntrinsicBlockSize(total_block_size);
  container_builder_.SetBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), container_builder_.Borders(),
                        &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizes NGMathFractionLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
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
