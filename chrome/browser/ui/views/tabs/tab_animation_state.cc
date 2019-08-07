// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_animation_state.h"

#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "ui/gfx/animation/tween.h"

TabAnimationState TabAnimationState::ForIdealTabState(TabOpenness open,
                                                      TabPinnedness pinned,
                                                      TabActiveness active,
                                                      int tab_index_offset) {
  return TabAnimationState(open == TabOpenness::kOpen ? 1 : 0,
                           pinned == TabPinnedness::kPinned ? 1 : 0,
                           active == TabActiveness::kActive ? 1 : 0,
                           tab_index_offset);
}

TabAnimationState TabAnimationState::Interpolate(float value,
                                                 TabAnimationState origin,
                                                 TabAnimationState target) {
  return TabAnimationState(
      gfx::Tween::FloatValueBetween(value, origin.openness_, target.openness_),
      gfx::Tween::FloatValueBetween(value, origin.pinnedness_,
                                    target.pinnedness_),
      gfx::Tween::FloatValueBetween(value, origin.activeness_,
                                    target.activeness_),
      gfx::Tween::FloatValueBetween(value, origin.normalized_leading_edge_x_,
                                    target.normalized_leading_edge_x_));
}

TabAnimationState TabAnimationState::WithOpenness(TabOpenness open) const {
  return TabAnimationState(open == TabOpenness::kOpen ? 1 : 0, pinnedness_,
                           activeness_, normalized_leading_edge_x_);
}

TabAnimationState TabAnimationState::WithPinnedness(
    TabPinnedness pinned) const {
  return TabAnimationState(openness_, pinned == TabPinnedness::kPinned ? 1 : 0,
                           activeness_, normalized_leading_edge_x_);
}

TabAnimationState TabAnimationState::WithActiveness(
    TabActiveness active) const {
  return TabAnimationState(openness_, pinnedness_,
                           active == TabActiveness::kActive ? 1 : 0,
                           normalized_leading_edge_x_);
}

int TabAnimationState::GetLeadingEdgeOffset(std::vector<int> tab_widths,
                                            int my_index) const {
  // TODO(949660): Implement this to handle animated tab translations. Sum
  // widths from my_index to my_index +
  // round_towards_zero(normalized_leading_edge_x), inclusive. Add the
  // fractional part for the width of the last tab.
  // A different approach that doesn't stretch/compress space based on
  // tab widths might be needed. Though maybe not, since very few animations
  // will actually translate tabs across a mixture of pinned and unpinned
  // tabs.
  NOTIMPLEMENTED();
  return 0;
}

bool TabAnimationState::IsFullyClosed() const {
  return openness_ == 0.0f;
}
