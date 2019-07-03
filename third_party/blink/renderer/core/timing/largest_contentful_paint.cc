// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LargestContentfulPaint::LargestContentfulPaint(double paint_time,
                                               uint64_t size,
                                               double response_end,
                                               const AtomicString& id,
                                               const String& url,
                                               Element* element)
    : PerformanceEntry(g_empty_atom, paint_time, paint_time),
      size_(size),
      response_end_(response_end),
      id_(id),
      url_(url),
      element_(element) {}

LargestContentfulPaint::~LargestContentfulPaint() = default;

AtomicString LargestContentfulPaint::entryType() const {
  return performance_entry_names::kLargestContentfulPaint;
}

PerformanceEntryType LargestContentfulPaint::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kLargestContentfulPaint;
}

Element* LargestContentfulPaint::element() const {
  if (!element_ || !element_->isConnected() || element_->IsInShadowTree())
    return nullptr;

  return element_;
}

void LargestContentfulPaint::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("size", size_);
  builder.Add("responseEnd", response_end_);
  builder.Add("id", id_);
  builder.Add("url", url_);
  builder.Add("element", element_);
}

void LargestContentfulPaint::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
