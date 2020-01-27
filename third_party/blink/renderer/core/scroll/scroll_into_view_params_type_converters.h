// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_INTO_VIEW_PARAMS_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_INTO_VIEW_PARAMS_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"

namespace mojo {

template <>
struct CORE_EXPORT TypeConverter<blink::mojom::blink::ScrollAlignmentPtr,
                                 blink::ScrollAlignment> {
  static blink::mojom::blink::ScrollAlignmentPtr Convert(
      const blink::ScrollAlignment& input);
};

template <>
struct CORE_EXPORT TypeConverter<blink::ScrollAlignment,
                                 blink::mojom::blink::ScrollAlignmentPtr> {
  static blink::ScrollAlignment Convert(
      const blink::mojom::blink::ScrollAlignmentPtr& input);
};

template <>
struct TypeConverter<blink::mojom::blink::ScrollIntoViewParams::Behavior,
                     blink::ScrollBehavior> {
  static blink::mojom::blink::ScrollIntoViewParams::Behavior Convert(
      blink::ScrollBehavior behavior);
};

template <>
struct TypeConverter<blink::ScrollBehavior,
                     blink::mojom::blink::ScrollIntoViewParams::Behavior> {
  static blink::ScrollBehavior Convert(
      blink::mojom::blink::ScrollIntoViewParams::Behavior behavior);
};

template <>
struct TypeConverter<blink::mojom::blink::ScrollIntoViewParams::Type,
                     blink::ScrollType> {
  static blink::mojom::blink::ScrollIntoViewParams::Type Convert(
      blink::ScrollType type);
};

template <>
struct TypeConverter<blink::ScrollType,
                     blink::mojom::blink::ScrollIntoViewParams::Type> {
  static blink::ScrollType Convert(
      blink::mojom::blink::ScrollIntoViewParams::Type type);
};

template <>
struct TypeConverter<blink::mojom::blink::ScrollAlignment::Behavior,
                     blink::ScrollAlignmentBehavior> {
  static blink::mojom::blink::ScrollAlignment::Behavior Convert(
      blink::ScrollAlignmentBehavior behavior);
};

template <>
struct TypeConverter<blink::ScrollAlignmentBehavior,
                     blink::mojom::blink::ScrollAlignment::Behavior> {
  static blink::ScrollAlignmentBehavior Convert(
      blink::mojom::blink::ScrollAlignment::Behavior type);
};

}  // namespace mojo

namespace blink {

CORE_EXPORT mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
    ScrollAlignment = ScrollAlignment::kAlignCenterIfNeeded,
    ScrollAlignment = ScrollAlignment::kAlignCenterIfNeeded,
    ScrollType scroll_type = kProgrammaticScroll,
    bool make_visible_in_visual_viewport = true,
    ScrollBehavior scroll_behavior = kScrollBehaviorAuto,
    bool is_for_scroll_sequence = false,
    bool zoom_into_rect = false);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_INTO_VIEW_PARAMS_TYPE_CONVERTERS_H_
