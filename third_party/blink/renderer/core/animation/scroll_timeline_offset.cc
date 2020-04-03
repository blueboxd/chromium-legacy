// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_scroll_timeline_element_based_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_element_based_offset.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

bool StringToScrollOffset(String scroll_offset,
                          const CSSParserContext& context,
                          CSSPrimitiveValue** result) {
  CSSTokenizer tokenizer(scroll_offset);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  CSSValue* value = css_parsing_utils::ConsumeScrollOffset(range, context);
  if (!value)
    return false;

  // We support 'auto', but for simplicity just store it as nullptr.
  *result = DynamicTo<CSSPrimitiveValue>(value);
  return true;
}

bool ValidateIntersectionBasedOffset(ScrollTimelineElementBasedOffset* offset) {
  if (!offset->hasTarget())
    return false;

  if (offset->hasThreshold()) {
    if (offset->threshold() < 0 || offset->threshold() > 1)
      return false;
  }

  return true;
}

}  // namespace

// static
ScrollTimelineOffset* ScrollTimelineOffset::Create(
    const StringOrScrollTimelineElementBasedOffset& input_offset,
    const CSSParserContext& context) {
  if (input_offset.IsString()) {
    CSSPrimitiveValue* offset = nullptr;
    if (!StringToScrollOffset(input_offset.GetAsString(), context, &offset)) {
      return nullptr;
    }

    return MakeGarbageCollected<ScrollTimelineOffset>(offset);
  } else if (input_offset.IsScrollTimelineElementBasedOffset()) {
    auto* offset = input_offset.GetAsScrollTimelineElementBasedOffset();
    if (!ValidateIntersectionBasedOffset(offset))
      return nullptr;

    return MakeGarbageCollected<ScrollTimelineOffset>(offset);
  } else {
    // The default case is "auto" which we initialized with null
    return MakeGarbageCollected<ScrollTimelineOffset>();
  }
}

double ScrollTimelineOffset::ResolveOffset(Node* scroll_source,
                                           ScrollOrientation orientation,
                                           double max_offset,
                                           double default_offset) {
  const LayoutBox* root_box = scroll_source->GetLayoutBox();
  DCHECK(root_box);
  Document& document = root_box->GetDocument();

  if (scroll_based_) {
    // Resolve scroll based offset.
    const ComputedStyle& computed_style = root_box->StyleRef();
    const ComputedStyle* root_style =
        document.documentElement()
            ? document.documentElement()->GetComputedStyle()
            : document.GetComputedStyle();

    CSSToLengthConversionData conversion_data = CSSToLengthConversionData(
        &computed_style, root_style, document.GetLayoutView(),
        computed_style.EffectiveZoom());
    double resolved = FloatValueForLength(
        scroll_based_->ConvertToLength(conversion_data), max_offset);

    return resolved;
  } else if (element_based) {
    // TODO(majidvp): turn element based info into an offset.
    // http://crbug.com/1023375
    return default_offset;
  } else {
    return default_offset;
  }
}

StringOrScrollTimelineElementBasedOffset
ScrollTimelineOffset::ToStringOrScrollTimelineElementBasedOffset() const {
  StringOrScrollTimelineElementBasedOffset result;
  if (scroll_based_) {
    result.SetString(scroll_based_->CssText());
  } else if (element_based) {
    result.SetScrollTimelineElementBasedOffset(element_based);
  } else {
    // we are dealing with default value
    result.SetString("auto");
  }

  return result;
}

ScrollTimelineOffset::ScrollTimelineOffset(CSSPrimitiveValue* offset)
    : scroll_based_(offset), element_based(nullptr) {}

ScrollTimelineOffset::ScrollTimelineOffset(
    ScrollTimelineElementBasedOffset* offset)
    : scroll_based_(nullptr), element_based(offset) {}

void ScrollTimelineOffset::Trace(blink::Visitor* visitor) {
  visitor->Trace(scroll_based_);
  visitor->Trace(element_based);
}

}  // namespace blink
