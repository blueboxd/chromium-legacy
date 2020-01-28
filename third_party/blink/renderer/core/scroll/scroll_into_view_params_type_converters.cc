// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_into_view_params_type_converters.h"

namespace mojo {

blink::mojom::blink::ScrollAlignmentPtr TypeConverter<
    blink::mojom::blink::ScrollAlignmentPtr,
    blink::ScrollAlignment>::Convert(const blink::ScrollAlignment& input) {
  auto output = blink::mojom::blink::ScrollAlignment::New();
  output->rect_visible = input.rect_visible_;
  output->rect_hidden = input.rect_hidden_;
  output->rect_partial = input.rect_partial_;
  return output;
}

blink::ScrollAlignment
TypeConverter<blink::ScrollAlignment, blink::mojom::blink::ScrollAlignmentPtr>::
    Convert(const blink::mojom::blink::ScrollAlignmentPtr& input) {
  blink::ScrollAlignment output;
  output.rect_visible_ = input->rect_visible;
  output.rect_hidden_ = input->rect_hidden;
  output.rect_partial_ = input->rect_partial;
  return output;
}

}  // namespace mojo

namespace blink {

mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
    ScrollAlignment align_x,
    ScrollAlignment align_y,
    mojom::blink::ScrollIntoViewParams::Type scroll_type,
    bool make_visible_in_visual_viewport,
    mojom::blink::ScrollIntoViewParams::Behavior scroll_behavior,
    bool is_for_scroll_sequence,
    bool zoom_into_rect) {
  auto params = mojom::blink::ScrollIntoViewParams::New();
  params->align_x = mojom::blink::ScrollAlignment::From(align_x);
  params->align_y = mojom::blink::ScrollAlignment::From(align_y);
  params->type = scroll_type;
  params->make_visible_in_visual_viewport = make_visible_in_visual_viewport;
  params->behavior = scroll_behavior;
  params->is_for_scroll_sequence = is_for_scroll_sequence;
  params->zoom_into_rect = zoom_into_rect;

  return params;
}

}  // namespace blink
