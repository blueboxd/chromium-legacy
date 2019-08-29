// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGColumnLayoutAlgorithm_h
#define NGColumnLayoutAlgorithm_h

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGConstraintSpace;
struct LogicalSize;

class CORE_EXPORT NGColumnLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGColumnLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  scoped_refptr<const NGLayoutResult> Layout() override;

  base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const override;

 private:
  // Lay out as many children as we can.
  void LayoutChildren();

  // Lay out one row of columns. The layout result returned is for the last
  // column that was laid out. The rows themselves don't create fragments.
  scoped_refptr<const NGLayoutResult> LayoutRow(
      const NGBlockBreakToken* next_column_token);

  // Lay out a column spanner. Will return a break token if we break before or
  // inside the spanner. If no break token is returned, it means that we can
  // proceed to the next row of columns.
  scoped_refptr<const NGBlockBreakToken> LayoutSpanner(
      NGBlockNode spanner_node,
      const NGBlockBreakToken* break_token);

  LayoutUnit CalculateBalancedColumnBlockSize(
      const LogicalSize& column_size,
      const NGBlockBreakToken* child_break_token);

  // Stretch the column length. We do this during column balancing, when we
  // discover that the current length isn't large enough to fit all content.
  LayoutUnit StretchColumnBlockSize(LayoutUnit minimal_space_shortage,
                                    LayoutUnit current_column_size) const;

  LayoutUnit ConstrainColumnBlockSize(LayoutUnit size) const;
  LayoutUnit CurrentContentBlockOffset() const;

  NGConstraintSpace CreateConstraintSpaceForColumns(
      const LogicalSize& column_size,
      bool is_first_fragmentainer,
      bool balance_columns) const;
  NGConstraintSpace CreateConstraintSpaceForBalancing(
      const LogicalSize& column_size) const;
  NGConstraintSpace CreateConstraintSpaceForSpanner(
      LayoutUnit block_offset) const;
  NGConstraintSpace CreateConstraintSpaceForMinMax() const;

  const NGBoxStrut border_padding_;
  const NGBoxStrut border_scrollbar_padding_;
  LogicalSize content_box_size_;
  int used_column_count_;
  LayoutUnit column_inline_size_;
  LayoutUnit column_inline_progression_;
  LayoutUnit intrinsic_block_size_;
};

}  // namespace blink

#endif  // NGColumnLayoutAlgorithm_h
