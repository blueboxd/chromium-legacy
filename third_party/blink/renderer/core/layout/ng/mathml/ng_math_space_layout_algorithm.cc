// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_space_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

NGMathSpaceLayoutAlgorithm::NGMathSpaceLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params),
      border_padding_(params.fragment_geometry.border +
                      params.fragment_geometry.padding) {
  DCHECK(params.fragment_geometry.scrollbar.IsEmpty());
  container_builder_.SetIsNewFormattingContext(true);
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

scoped_refptr<const NGLayoutResult> NGMathSpaceLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), border_padding_, border_padding_.BlockSum(),
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(border_padding_.BlockSum());
  container_builder_.SetBlockSize(block_size);

  container_builder_.SetBaseline(
      border_padding_.block_start +
      ValueForLength(Style().GetMathBaseline(), LayoutUnit()));
  return container_builder_.ToBoxFragment();
}

MinMaxSizes NGMathSpaceLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  if (auto sizes =
          CalculateMinMaxSizesIgnoringChildren(Node(), border_padding_))
    return *sizes;

  MinMaxSizes sizes;
  sizes += border_padding_.InlineSum();
  return sizes;
}

}  // namespace blink
