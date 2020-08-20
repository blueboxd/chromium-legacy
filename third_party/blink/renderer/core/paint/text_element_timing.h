// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_ELEMENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_ELEMENT_TIMING_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class IntRect;
class LocalFrameView;
class PropertyTreeStateOrAlias;
class TextRecord;

// TextElementTiming is responsible for tracking the paint timings for groups of
// text nodes associated with elements of a given window.
class CORE_EXPORT TextElementTiming final
    : public GarbageCollected<TextElementTiming>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  explicit TextElementTiming(LocalDOMWindow&);
  TextElementTiming(const TextElementTiming&) = delete;
  TextElementTiming& operator=(const TextElementTiming&) = delete;

  static TextElementTiming& From(LocalDOMWindow&);

  static inline bool NeededForElementTiming(Node& node) {
    auto* element = DynamicTo<Element>(node);
    return !node.IsInShadowTree() && element &&
           element->FastHasAttribute(html_names::kElementtimingAttr);
  }

  static FloatRect ComputeIntersectionRect(
      const LayoutObject&,
      const IntRect& aggregated_visual_rect,
      const PropertyTreeStateOrAlias&,
      const LocalFrameView*);

  bool CanReportElements() const;

  // Called when the swap promise queued by TextPaintTimingDetector has been
  // resolved. Dispatches PerformanceElementTiming entries to WindowPerformance.
  void OnTextObjectPainted(const TextRecord&);

  void Trace(Visitor* visitor) const override;

  Member<WindowPerformance> performance_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_ELEMENT_TIMING_H_
