// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_into_view_params_type_converters.h"

namespace mojo {

blink::mojom::blink::ScrollAlignmentPtr TypeConverter<
    blink::mojom::blink::ScrollAlignmentPtr,
    blink::ScrollAlignment>::Convert(const blink::ScrollAlignment& input) {
  auto output = blink::mojom::blink::ScrollAlignment::New();
  output->rect_visible =
      ConvertTo<blink::mojom::blink::ScrollAlignment::Behavior>(
          input.rect_visible_);
  output->rect_hidden =
      ConvertTo<blink::mojom::blink::ScrollAlignment::Behavior>(
          input.rect_hidden_);
  output->rect_partial =
      ConvertTo<blink::mojom::blink::ScrollAlignment::Behavior>(
          input.rect_partial_);
  return output;
}

blink::ScrollAlignment
TypeConverter<blink::ScrollAlignment, blink::mojom::blink::ScrollAlignmentPtr>::
    Convert(const blink::mojom::blink::ScrollAlignmentPtr& input) {
  blink::ScrollAlignment output;
  output.rect_visible_ =
      ConvertTo<blink::ScrollAlignmentBehavior>(input->rect_visible);
  output.rect_hidden_ =
      ConvertTo<blink::ScrollAlignmentBehavior>(input->rect_hidden);
  output.rect_partial_ =
      ConvertTo<blink::ScrollAlignmentBehavior>(input->rect_partial);
  return output;
}

// static
blink::mojom::blink::ScrollIntoViewParams::Type
TypeConverter<blink::mojom::blink::ScrollIntoViewParams::Type,
              blink::ScrollType>::Convert(blink::ScrollType type) {
  switch (type) {
    case blink::kUserScroll:
      return blink::mojom::blink::ScrollIntoViewParams::Type::kUser;
    case blink::kProgrammaticScroll:
      return blink::mojom::blink::ScrollIntoViewParams::Type::kProgrammatic;
    case blink::kClampingScroll:
      return blink::mojom::blink::ScrollIntoViewParams::Type::kClamping;
    case blink::kCompositorScroll:
      return blink::mojom::blink::ScrollIntoViewParams::Type::kCompositor;
    case blink::kAnchoringScroll:
      return blink::mojom::blink::ScrollIntoViewParams::Type::kAnchoring;
    case blink::kSequencedScroll:
      return blink::mojom::blink::ScrollIntoViewParams::Type::kSequenced;
  }
  NOTREACHED();
  return blink::mojom::blink::ScrollIntoViewParams::Type::kUser;
}

// static
blink::ScrollType
TypeConverter<blink::ScrollType,
              blink::mojom::blink::ScrollIntoViewParams::Type>::
    Convert(blink::mojom::blink::ScrollIntoViewParams::Type type) {
  switch (type) {
    case blink::mojom::blink::ScrollIntoViewParams::Type::kUser:
      return blink::kUserScroll;
    case blink::mojom::blink::ScrollIntoViewParams::Type::kProgrammatic:
      return blink::kProgrammaticScroll;
    case blink::mojom::blink::ScrollIntoViewParams::Type::kClamping:
      return blink::kClampingScroll;
    case blink::mojom::blink::ScrollIntoViewParams::Type::kCompositor:
      return blink::kCompositorScroll;
    case blink::mojom::blink::ScrollIntoViewParams::Type::kAnchoring:
      return blink::kAnchoringScroll;
    case blink::mojom::blink::ScrollIntoViewParams::Type::kSequenced:
      return blink::kSequencedScroll;
  }
  NOTREACHED();
  return blink::kUserScroll;
}

// static
blink::mojom::blink::ScrollAlignment::Behavior TypeConverter<
    blink::mojom::blink::ScrollAlignment::Behavior,
    blink::ScrollAlignmentBehavior>::Convert(blink::ScrollAlignmentBehavior
                                                 alignment) {
  switch (alignment) {
    case blink::kScrollAlignmentNoScroll:
      return blink::mojom::blink::ScrollAlignment::Behavior::kNoScroll;
    case blink::kScrollAlignmentCenter:
      return blink::mojom::blink::ScrollAlignment::Behavior::kCenter;
    case blink::kScrollAlignmentTop:
      return blink::mojom::blink::ScrollAlignment::Behavior::kTop;
    case blink::kScrollAlignmentBottom:
      return blink::mojom::blink::ScrollAlignment::Behavior::kBottom;
    case blink::kScrollAlignmentLeft:
      return blink::mojom::blink::ScrollAlignment::Behavior::kLeft;
    case blink::kScrollAlignmentRight:
      return blink::mojom::blink::ScrollAlignment::Behavior::kRight;
    case blink::kScrollAlignmentClosestEdge:
      return blink::mojom::blink::ScrollAlignment::Behavior::kClosestEdge;
  }
  NOTREACHED();
  return blink::mojom::blink::ScrollAlignment::Behavior::kNoScroll;
}

// static
blink::ScrollAlignmentBehavior
TypeConverter<blink::ScrollAlignmentBehavior,
              blink::mojom::blink::ScrollAlignment::Behavior>::
    Convert(blink::mojom::blink::ScrollAlignment::Behavior alignment) {
  switch (alignment) {
    case blink::mojom::blink::ScrollAlignment::Behavior::kNoScroll:
      return blink::kScrollAlignmentNoScroll;
    case blink::mojom::blink::ScrollAlignment::Behavior::kCenter:
      return blink::kScrollAlignmentCenter;
    case blink::mojom::blink::ScrollAlignment::Behavior::kTop:
      return blink::kScrollAlignmentTop;
    case blink::mojom::blink::ScrollAlignment::Behavior::kBottom:
      return blink::kScrollAlignmentBottom;
    case blink::mojom::blink::ScrollAlignment::Behavior::kLeft:
      return blink::kScrollAlignmentLeft;
    case blink::mojom::blink::ScrollAlignment::Behavior::kRight:
      return blink::kScrollAlignmentRight;
    case blink::mojom::blink::ScrollAlignment::Behavior::kClosestEdge:
      return blink::kScrollAlignmentClosestEdge;
  }
  NOTREACHED();
  return blink::kScrollAlignmentNoScroll;
}

}  // namespace mojo

namespace blink {

mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
    ScrollAlignment align_x,
    ScrollAlignment align_y,
    ScrollType scroll_type,
    bool make_visible_in_visual_viewport,
    mojom::blink::ScrollIntoViewParams::Behavior scroll_behavior,
    bool is_for_scroll_sequence,
    bool zoom_into_rect) {
  auto params = mojom::blink::ScrollIntoViewParams::New();
  params->align_x = mojom::blink::ScrollAlignment::From(align_x);
  params->align_y = mojom::blink::ScrollAlignment::From(align_y);
  params->type =
      mojo::ConvertTo<mojom::blink::ScrollIntoViewParams::Type>(scroll_type);
  params->make_visible_in_visual_viewport = make_visible_in_visual_viewport;
  params->behavior = scroll_behavior;
  params->is_for_scroll_sequence = is_for_scroll_sequence;
  params->zoom_into_rect = zoom_into_rect;

  return params;
}

}  // namespace blink
