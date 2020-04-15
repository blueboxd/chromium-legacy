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

bool ValidateElementBasedOffset(ScrollTimelineElementBasedOffset* offset) {
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
    if (!ValidateElementBasedOffset(offset))
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

  if (length_based_offset_) {
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
        length_based_offset_->ConvertToLength(conversion_data), max_offset);

    return resolved;
  } else if (element_based_offset_) {
    // We assume that the root is the target's ancestor in layout tree. Under
    // this assumption |target.LocalToAncestorRect()| returns the targets's
    // position relative to the root's border box, while ignoring scroll offset.
    //
    // TODO(majidvp): We need to validate this assumption and deal with cases
    // where it is not true. See the spec discussion here:
    // https://github.com/w3c/csswg-drafts/issues/4337#issuecomment-610989843

    DCHECK(element_based_offset_->hasTarget());
    Element* target = element_based_offset_->target();
    const LayoutBox* target_box = target->GetLayoutBox();

    // It is possible for target to not have a layout box e.g., if it is an
    // unattached element. In which case we return the default offset for now.
    //
    // TODO(majidvp): Need to consider this case in the spec. Most likely we
    // should remain unresolved. See the spec discussion here:
    // https://github.com/w3c/csswg-drafts/issues/4337#issuecomment-610997231
    if (!target_box) {
      return default_offset;
    }

    PhysicalRect target_rect = target_box->PhysicalBorderBoxRect();
    target_rect = target_box->LocalToAncestorRect(
        target_rect, root_box,
        kTraverseDocumentBoundaries | kIgnoreScrollOffset);

    PhysicalRect root_rect(root_box->PhysicalBorderBoxRect());

    LayoutUnit root_edge;
    LayoutUnit target_edge;

    // Here is the simple diagram that shows the computation.
    //
    //                 +-----+
    //                 |     |     +------+
    //                 |     |     |      |
    // edge:start +----+-----+-------------------+-----+-------+
    //            |                |xxxxxx|      |xxxxx|       |
    //            |                +------+      |xxxxx|       |
    //            |                              +-----+       |
    //            |                                            |
    // threshold: |    A) 0       B) 0.5         C) 1          |
    //            |                                            |
    //            |                              +-----+       |
    //            |                +------+      |xxxxx|       |
    //            |                |xxxxxx|      |xxxxx|       |
    // edge: end  +----+-----+-------------------+-----+-------+
    //                 |     |     |      |
    //                 |     |     +------+
    //                 +-----+
    //
    // We always take the target top edge and compute the distance to the
    // root's selected edge. This give us (C) in start edge case and (A) in
    // end edge case.
    //
    // To take threshold into account we simply add (1-threshold) or threshold
    // in start and end edge cases respectively.
    bool is_start = element_based_offset_->edge() == "start";
    float threshold_adjustment = is_start
                                     ? (1 - element_based_offset_->threshold())
                                     : element_based_offset_->threshold();

    if (orientation == kVerticalScroll) {
      root_edge = is_start ? root_rect.Y() : root_rect.Bottom();
      target_edge = target_rect.Y();
      // Note that threshold is considered as a portion of target and not as a
      // portion of root. IntersectionObserver has option to allow both.
      target_edge += (threshold_adjustment * target_rect.Height());
    } else {  // kHorizontalScroll
      root_edge = is_start ? root_rect.X() : root_rect.Right();
      target_edge = target_rect.X();
      target_edge += (threshold_adjustment * target_rect.Width());
    }

    // TODO(majidvp): Potentially clip by min/max scroll offsets.
    // http://crbug.com/1023375
    return (target_edge - root_edge).ToDouble();
  } else {
    // Resolve the default case (i.e., 'auto' value)
    return default_offset;
  }
}

StringOrScrollTimelineElementBasedOffset
ScrollTimelineOffset::ToStringOrScrollTimelineElementBasedOffset() const {
  StringOrScrollTimelineElementBasedOffset result;
  if (length_based_offset_) {
    result.SetString(length_based_offset_->CssText());
  } else if (element_based_offset_) {
    result.SetScrollTimelineElementBasedOffset(element_based_offset_);
  } else {
    // This is the default value (i.e., 'auto' value)
    result.SetString("auto");
  }

  return result;
}

ScrollTimelineOffset::ScrollTimelineOffset(CSSPrimitiveValue* offset)
    : length_based_offset_(offset), element_based_offset_(nullptr) {}

ScrollTimelineOffset::ScrollTimelineOffset(
    ScrollTimelineElementBasedOffset* offset)
    : length_based_offset_(nullptr), element_based_offset_(offset) {}

void ScrollTimelineOffset::Trace(blink::Visitor* visitor) {
  visitor->Trace(length_based_offset_);
  visitor->Trace(element_based_offset_);
}

}  // namespace blink
