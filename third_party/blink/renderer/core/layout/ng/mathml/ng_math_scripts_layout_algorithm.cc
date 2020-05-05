// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_scripts_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"

namespace blink {
namespace {

using MathConstants = OpenTypeMathSupport::MathConstants;

LayoutUnit GetSpaceAfterScript(const ComputedStyle& style) {
  return LayoutUnit(MathConstant(style, MathConstants::kSpaceAfterScript)
                        .value_or(style.FontSize() / 5));
}

// Describes the amount of shift to apply to the sub/sup boxes.
// Data is populated from the OpenType MATH table.
// If the OpenType MATH table is not present fallback values are used.
// https://mathml-refresh.github.io/mathml-core/#base-with-subscript
// https://mathml-refresh.github.io/mathml-core/#base-with-superscript
// https://mathml-refresh.github.io/mathml-core/#base-with-subscript-and-superscript
struct ScriptsVerticalParameters {
  STACK_ALLOCATED();

 public:
  LayoutUnit subscript_shift_down;
  LayoutUnit superscript_shift_up;
  LayoutUnit superscript_shift_up_cramped;
  LayoutUnit subscript_baseline_drop_min;
  LayoutUnit superscript_baseline_drop_max;
  LayoutUnit sub_superscript_gap_min;
  LayoutUnit superscript_bottom_min;
  LayoutUnit subscript_top_max;
  LayoutUnit superscript_bottom_max_with_subscript;
};

ScriptsVerticalParameters GetScriptsVerticalParameters(
    const ComputedStyle& style) {
  ScriptsVerticalParameters parameters;
  auto x_height = style.GetFont().PrimaryFont()->GetFontMetrics().XHeight();
  parameters.subscript_shift_down =
      LayoutUnit(MathConstant(style, MathConstants::kSubscriptShiftDown)
                     .value_or(x_height / 3));
  parameters.superscript_shift_up =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptShiftUp)
                     .value_or(x_height));
  parameters.superscript_shift_up_cramped =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptShiftUpCramped)
                     .value_or(x_height));
  parameters.subscript_baseline_drop_min =
      LayoutUnit(MathConstant(style, MathConstants::kSubscriptBaselineDropMin)
                     .value_or(x_height / 2));
  parameters.superscript_baseline_drop_max =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptBaselineDropMax)
                     .value_or(x_height / 2));
  parameters.sub_superscript_gap_min =
      LayoutUnit(MathConstant(style, MathConstants::kSubSuperscriptGapMin)
                     .value_or(style.FontSize() / 5));
  parameters.superscript_bottom_min =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptBottomMin)
                     .value_or(x_height / 4));
  parameters.subscript_top_max =
      LayoutUnit(MathConstant(style, MathConstants::kSubscriptTopMax)
                     .value_or(4 * x_height / 5));
  parameters.superscript_bottom_max_with_subscript = LayoutUnit(
      MathConstant(style, MathConstants::kSuperscriptBottomMaxWithSubscript)
          .value_or(4 * x_height / 5));
  return parameters;
}

}  // namespace

NGMathScriptsLayoutAlgorithm::NGMathScriptsLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_scrollbar_padding_(params.fragment_geometry.border +
                                params.fragment_geometry.scrollbar +
                                params.fragment_geometry.padding) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
  child_available_size_ = ShrinkAvailableSize(
      container_builder_.InitialBorderBoxSize(), border_scrollbar_padding_);
}

void NGMathScriptsLayoutAlgorithm::GatherChildren(
    NGBlockNode* base,
    NGBlockNode* sub,
    NGBlockNode* sup,
    NGBoxFragmentBuilder* container_builder) const {
  auto script_type = Node().ScriptType();
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    NGBlockNode block_child = To<NGBlockNode>(child);
    if (child.IsOutOfFlowPositioned()) {
      if (container_builder) {
        container_builder->AddOutOfFlowChildCandidate(
            block_child, {border_scrollbar_padding_.inline_start,
                          border_scrollbar_padding_.block_start});
      }
      continue;
    }
    if (!*base) {
      // All scripted elements must have at least one child.
      // The first child is the base.
      *base = block_child;
      continue;
    }
    switch (script_type) {
      case MathScriptType::kSub:
        // These elements must have exactly two children.
        // The second child is a postscript and there are no prescripts.
        // <msub> base subscript </msub>
        // <msup> base superscript </msup>
        DCHECK(!*sub);
        *sub = block_child;
        continue;
      case MathScriptType::kSuper:
        DCHECK(!*sup);
        *sup = block_child;
        continue;
      case MathScriptType::kSubSup:
        // These elements must have exactly three children.
        // The second and third children are postscripts and there are no
        // prescripts. <msubsup> base subscript superscript </msubsup>
        if (!*sub) {
          *sub = block_child;
        } else {
          DCHECK(!*sup);
          *sup = block_child;
        }
        continue;
      default:
        NOTREACHED();
    }
  }
}

// Determines ascent/descent and shift metrics depending on script type.
NGMathScriptsLayoutAlgorithm::VerticalMetrics
NGMathScriptsLayoutAlgorithm::GetVerticalMetrics(
    const ChildAndMetrics& base_metrics,
    const ChildAndMetrics& sub_metrics,
    const ChildAndMetrics& sup_metrics) const {
  ScriptsVerticalParameters parameters = GetScriptsVerticalParameters(Style());
  VerticalMetrics metrics;

  MathScriptType type = Node().ScriptType();
  if (type == MathScriptType::kSub || type == MathScriptType::kSubSup) {
    metrics.sub_shift =
        std::max(parameters.subscript_shift_down,
                 base_metrics.descent + parameters.subscript_baseline_drop_min);
  }
  LayoutUnit shift_up = parameters.superscript_shift_up;
  if (type == MathScriptType::kSuper || type == MathScriptType::kSubSup) {
    // TODO(rbuis): test cramped for super/subSup.
    metrics.sup_shift =
        std::max(shift_up, base_metrics.ascent -
                               parameters.superscript_baseline_drop_max);
  }

  switch (type) {
    case MathScriptType::kSub: {
      metrics.descent = sub_metrics.descent;
      metrics.sub_shift = std::max(
          metrics.sub_shift, sub_metrics.ascent - parameters.subscript_top_max);
    } break;
    case MathScriptType::kSuper: {
      metrics.ascent = sup_metrics.ascent;
      metrics.sup_shift =
          std::max(metrics.sup_shift,
                   parameters.superscript_bottom_min + sup_metrics.descent);
    } break;
    case MathScriptType::kSubSup: {
      metrics.ascent = std::max(metrics.ascent, sup_metrics.ascent);
      metrics.descent = std::max(metrics.descent, sub_metrics.descent);
      LayoutUnit sub_script_shift = std::max(
          parameters.subscript_shift_down,
          base_metrics.descent + parameters.subscript_baseline_drop_min);
      sub_script_shift = std::max(
          sub_script_shift, sub_metrics.ascent - parameters.subscript_top_max);
      LayoutUnit sup_script_shift =
          std::max(shift_up, base_metrics.ascent -
                                 parameters.superscript_baseline_drop_max);
      sup_script_shift =
          std::max(sup_script_shift,
                   parameters.superscript_bottom_min + sup_metrics.descent);

      LayoutUnit sub_super_script_gap =
          (sub_script_shift - sub_metrics.ascent) +
          (sup_script_shift - sup_metrics.descent);
      if (sub_super_script_gap < parameters.sub_superscript_gap_min) {
        // First, we try and push the superscript up.
        LayoutUnit delta = parameters.superscript_bottom_max_with_subscript -
                           (sup_script_shift - sup_metrics.descent);
        if (delta > 0) {
          delta = std::min(
              delta, parameters.sub_superscript_gap_min - sub_super_script_gap);
          sup_script_shift += delta;
          sub_super_script_gap += delta;
        }
        // If that is not enough, we push the subscript down.
        if (sub_super_script_gap < parameters.sub_superscript_gap_min) {
          sub_script_shift +=
              parameters.sub_superscript_gap_min - sub_super_script_gap;
        }
      }

      metrics.sub_shift = std::max(metrics.sub_shift, sub_script_shift);
      metrics.sup_shift = std::max(metrics.sup_shift, sup_script_shift);
    } break;
    case MathScriptType::kOver:
    case MathScriptType::kUnder:
    case MathScriptType::kUnderOver:
      // TODO(rbuis): implement movablelimits.
      NOTREACHED();
      break;
  }

  return metrics;
}

NGMathScriptsLayoutAlgorithm::ChildAndMetrics
NGMathScriptsLayoutAlgorithm::LayoutAndGetMetrics(NGBlockNode child) const {
  ChildAndMetrics child_and_metrics;
  auto constraint_space = CreateConstraintSpaceForMathChild(
      Node(), child_available_size_, ConstraintSpace(), child);
  child_and_metrics.result =
      child.Layout(constraint_space, nullptr /*break_token*/);
  NGBoxFragment fragment(
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
      To<NGPhysicalBoxFragment>(child_and_metrics.result->PhysicalFragment()));
  child_and_metrics.inline_size = fragment.InlineSize();
  child_and_metrics.margins =
      ComputeMarginsFor(constraint_space, child.Style(), ConstraintSpace());
  child_and_metrics.ascent = fragment.Baseline().value_or(fragment.BlockSize());
  child_and_metrics.descent = fragment.BlockSize() - child_and_metrics.ascent +
                              child_and_metrics.margins.block_end;
  child_and_metrics.ascent += child_and_metrics.margins.block_start;
  return child_and_metrics;
}

scoped_refptr<const NGLayoutResult> NGMathScriptsLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  NGBlockNode base = nullptr;
  NGBlockNode sub = nullptr;
  NGBlockNode sup = nullptr;
  GatherChildren(&base, &sub, &sup, &container_builder_);

  ChildAndMetrics base_metrics = LayoutAndGetMetrics(base);
  ChildAndMetrics sub_metrics, sup_metrics;
  if (sub)
    sub_metrics = LayoutAndGetMetrics(sub);
  if (sup)
    sup_metrics = LayoutAndGetMetrics(sup);
  VerticalMetrics metrics =
      GetVerticalMetrics(base_metrics, sub_metrics, sup_metrics);

  LayoutUnit ascent =
      std::max(base_metrics.ascent, metrics.ascent + metrics.sup_shift) +
      border_scrollbar_padding_.block_start;
  LayoutUnit descent =
      std::max(base_metrics.descent, metrics.descent + metrics.sub_shift);
  // TODO(rbuis): take into account italic correction.
  LayoutUnit inline_offset = border_scrollbar_padding_.inline_start +
                             base_metrics.margins.inline_start;

  LogicalOffset base_offset(
      inline_offset,
      ascent - base_metrics.ascent + base_metrics.margins.block_start);
  container_builder_.AddChild(base_metrics.result->PhysicalFragment(),
                              base_offset);
  base.StoreMargins(ConstraintSpace(), base_metrics.margins);
  inline_offset += base_metrics.inline_size + base_metrics.margins.inline_end;

  if (sub) {
    LogicalOffset sub_offset(inline_offset + sub_metrics.margins.inline_start,
                             ascent + metrics.sub_shift - sub_metrics.ascent +
                                 sub_metrics.margins.block_start);
    container_builder_.AddChild(sub_metrics.result->PhysicalFragment(),
                                sub_offset);
    sub.StoreMargins(ConstraintSpace(), sub_metrics.margins);
  }
  if (sup) {
    LogicalOffset sup_offset(inline_offset + sup_metrics.margins.inline_start,
                             ascent - metrics.sup_shift - sup_metrics.ascent +
                                 sup_metrics.margins.block_start);
    container_builder_.AddChild(sup_metrics.result->PhysicalFragment(),
                                sup_offset);
    sup.StoreMargins(ConstraintSpace(), sup_metrics.margins);
  }

  container_builder_.SetBaseline(ascent);

  LayoutUnit intrinsic_block_size =
      ascent + descent + border_scrollbar_padding_.block_end;

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_scrollbar_padding_,
      intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), container_builder_.Borders(),
                        &container_builder_)
      .Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizes NGMathScriptsLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  if (auto sizes = CalculateMinMaxSizesIgnoringChildren(
          Node(), border_scrollbar_padding_))
    return *sizes;

  NGBlockNode base = nullptr;
  NGBlockNode sub = nullptr;
  NGBlockNode sup = nullptr;
  GatherChildren(&base, &sub, &sup);

  LayoutUnit child_percentage_resolution_block_size =
      CalculateChildPercentageBlockSizeForMinMax(
          ConstraintSpace(), Node(), border_scrollbar_padding_,
          input.percentage_resolution_block_size);

  MinMaxSizesInput child_input(child_percentage_resolution_block_size);

  MinMaxSizes sizes;
  sizes += ComputeMinAndMaxContentContribution(Style(), base, child_input);
  sizes += ComputeMinMaxMargins(Style(), base).InlineSum();
  sizes += GetSpaceAfterScript(Style());
  switch (Node().ScriptType()) {
    case MathScriptType::kSub:
    case MathScriptType::kSuper: {
      // TODO(fwang): Take italic correction into account.
      auto first_post_script = sub ? sub : sup;
      sizes += ComputeMinAndMaxContentContribution(Style(), first_post_script,
                                                   child_input);
      sizes += ComputeMinMaxMargins(Style(), first_post_script).InlineSum();
      break;
    }
    case MathScriptType::kSubSup: {
      // TODO(fwang): Take italic correction into account.
      MinMaxSizes sub_sup_pair_size;
      auto sub_size =
          ComputeMinAndMaxContentContribution(Style(), sub, child_input);
      sub_size += ComputeMinMaxMargins(Style(), sub).InlineSum();
      sub_sup_pair_size.Encompass(sub_size);
      auto sup_size =
          ComputeMinAndMaxContentContribution(Style(), sup, child_input);
      sup_size += ComputeMinMaxMargins(Style(), sup).InlineSum();
      sub_sup_pair_size.Encompass(sup_size);
      sizes += sub_sup_pair_size;
      break;
    }
    case MathScriptType::kUnder:
    case MathScriptType::kOver:
    case MathScriptType::kUnderOver:
      // TODO(rbuis): implement movablelimits.
      NOTREACHED();
      break;
  }
  sizes += border_scrollbar_padding_.InlineSum();
  return sizes;
}

}  // namespace blink
