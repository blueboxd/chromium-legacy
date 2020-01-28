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

}  // namespace mojo

namespace blink {

CORE_EXPORT mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
    ScrollAlignment = ScrollAlignment::kAlignCenterIfNeeded,
    ScrollAlignment = ScrollAlignment::kAlignCenterIfNeeded,
    mojom::blink::ScrollIntoViewParams::Type scroll_type =
        mojom::blink::ScrollIntoViewParams::Type::kProgrammatic,
    bool make_visible_in_visual_viewport = true,
    mojom::blink::ScrollIntoViewParams::Behavior scroll_behavior =
        mojom::blink::ScrollIntoViewParams::Behavior::kAuto,
    bool is_for_scroll_sequence = false,
    bool zoom_into_rect = false);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_INTO_VIEW_PARAMS_TYPE_CONVERTERS_H_
