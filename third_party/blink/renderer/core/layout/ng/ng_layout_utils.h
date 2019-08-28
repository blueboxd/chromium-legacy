// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_UTILS_H_

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

namespace blink {

class NGConstraintSpace;
class NGLayoutResult;

// NGLayoutCacheStatus indicates what type of cache hit/miss occurred. For
// various types of misses we may be able to perform less work than a full
// layout.
//
// See |NGSimplifiedLayoutAlgorithm| for details about the
// |kNeedsSimplifiedLayout| cache miss type.
enum class NGLayoutCacheStatus {
  kHit,                   // Cache hit, no additional work required.
  kNeedsLayout,           // Cache miss, full layout required.
  kNeedsSimplifiedLayout  // Cache miss, simplified layout required.
};

// Calculates the |NGLayoutCacheStatus| based on sizing information. Returns:
//  - |NGLayoutCacheStatus::kHit| if the size will be the same as
//    |cached_layout_result|, and therefore might be able to skip layout.
//  - |NGLayoutCacheStatus::kNeedsSimplifiedLayout| if a simplified layout may
//    be possible (just based on the sizing information at this point).
//  - |NGLayoutCacheStatus::kNeedsLayout| if a full layout is required.
//
// May pre-compute the |fragment_geometry| while calculating this status.
NGLayoutCacheStatus CalculateSizeBasedLayoutCacheStatus(
    const NGBlockNode& node,
    const NGLayoutResult& cached_layout_result,
    const NGConstraintSpace& new_space,
    base::Optional<NGFragmentGeometry>* fragment_geometry);

// Similar to |MaySkipLayout| but for legacy layout roots. Doesn't attempt to
// pre-compute the geometry of the fragment.
bool MaySkipLegacyLayout(const NGBlockNode& node,
                         const NGLayoutResult& cached_layout_result,
                         const NGConstraintSpace& new_space);

// Returns true if for a given |new_space|, the |cached_layout_result| won't be
// affected by clearance, or floats, and therefore might be able to skip
// layout.
// Additionally (if this function returns true) it will calculate the new
// |bfc_block_offset|, and |block_offset_delta| for the layout result.
// |bfc_block_offset| may still be |base::nullopt| if not previously set.
//
// If this function returns false, both |bfc_block_offset|, and
// |block_offset_delta| are in an undefined state and should not be used.
bool MaySkipLayoutWithinBlockFormattingContext(
    const NGLayoutResult& cached_layout_result,
    const NGConstraintSpace& new_space,
    base::Optional<LayoutUnit>* bfc_block_offset,
    LayoutUnit* block_offset_delta);

// Return true if layout is considered complete. In some cases we require more
// than one layout pass.
// This function never considers intermediate layouts with
// |NGConstraintSpace::IsIntermediateLayout| to be complete.
bool IsBlockLayoutComplete(const NGConstraintSpace&, const NGLayoutResult&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_UTILS_H_
