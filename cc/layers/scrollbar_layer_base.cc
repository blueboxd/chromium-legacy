// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_base.h"

#include "cc/layers/scrollbar_layer_impl_base.h"

namespace cc {

ScrollbarLayerBase::ScrollbarLayerBase(ScrollbarOrientation orientation,
                                       bool is_left_side_vertical_scrollbar)
    : orientation_(orientation),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar) {
  SetIsScrollbar(true);
}

ScrollbarLayerBase::~ScrollbarLayerBase() = default;

void ScrollbarLayerBase::SetScrollElementId(ElementId element_id) {
  if (element_id == scroll_element_id_)
    return;

  scroll_element_id_ = element_id;
  SetNeedsCommit();
}

void ScrollbarLayerBase::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  auto* scrollbar_layer_impl = static_cast<ScrollbarLayerImplBase*>(layer);
  DCHECK_EQ(scrollbar_layer_impl->orientation(), orientation_);
  DCHECK_EQ(scrollbar_layer_impl->is_left_side_vertical_scrollbar(),
            is_left_side_vertical_scrollbar_);
  scrollbar_layer_impl->SetScrollElementId(scroll_element_id_);
}

}  // namespace cc
