// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_grid_layout_algorithm.h"

namespace blink {

NGGridLayoutAlgorithm::NGGridLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  DCHECK(!params.break_token);
  container_builder_.SetIsNewFormattingContext(true);
  container_builder_.SetInitialFragmentGeometry(params.fragment_geometry);
}

scoped_refptr<const NGLayoutResult> NGGridLayoutAlgorithm::Layout() {
  return container_builder_.ToBoxFragment();
}

MinMaxSizes NGGridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  return MinMaxSizes();
}

}  // namespace blink
