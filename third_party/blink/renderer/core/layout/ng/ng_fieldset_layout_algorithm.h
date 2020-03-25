// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

enum class NGBreakStatus;
class NGBlockBreakToken;
class NGConstraintSpace;

class CORE_EXPORT NGFieldsetLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGFieldsetLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  base::Optional<MinMaxSizes> ComputeMinMaxSizes(
      const MinMaxSizesInput&) const override;

 private:
  NGBreakStatus LayoutChildren();
  NGBreakStatus LayoutLegend(
      NGBlockNode& legend,
      scoped_refptr<const NGBlockBreakToken> legend_break_token);
  NGBreakStatus LayoutFieldsetContent(
      NGBlockNode& fieldset_content,
      scoped_refptr<const NGBlockBreakToken> content_break_token,
      LogicalSize adjusted_padding_box_size,
      LayoutUnit fragmentainer_block_offset,
      bool has_legend);

  const NGConstraintSpace CreateConstraintSpaceForLegend(
      NGBlockNode legend,
      LogicalSize available_size,
      LogicalSize percentage_size,
      LayoutUnit block_offset);
  const NGConstraintSpace CreateConstraintSpaceForFieldsetContent(
      NGBlockNode fieldset_content,
      LogicalSize padding_box_size,
      LayoutUnit block_offset);

  bool IsFragmentainerOutOfSpace(LayoutUnit block_offset) const;

  const WritingMode writing_mode_;

  const NGBoxStrut border_padding_;
  NGBoxStrut borders_;
  NGBoxStrut padding_;

  // The border and padding after adjusting to ensure that the leading border
  // and padding are only applied to the first fragment.
  NGBoxStrut adjusted_border_padding_;

  // The result of borders_ after positioning the fieldset's legend element.
  NGBoxStrut borders_with_legend_;

  LayoutUnit block_start_padding_edge_;
  LayoutUnit intrinsic_block_size_;
  const LayoutUnit consumed_block_size_;
  LogicalSize border_box_size_;

  // The legend may eat from the available content box block size. This
  // represents the minimum block size needed by the border box to encompass
  // the legend.
  LayoutUnit minimum_border_box_block_size_;

  // If true, this indicates the block_start_padding_edge_ had changed from its
  // initial value during the current layout pass.
  bool block_start_padding_edge_adjusted_ = false;

  // If true, this indicates that the legend broke during the current layout
  // pass.
  bool legend_broke_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FIELDSET_LAYOUT_ALGORITHM_H_
