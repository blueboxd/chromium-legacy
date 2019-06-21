// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LargestContentfulPaint::LargestContentfulPaint(double paint_time, uint64_t size)
    : PerformanceEntry(g_empty_atom, paint_time, paint_time), size_(size) {}

LargestContentfulPaint::~LargestContentfulPaint() = default;

AtomicString LargestContentfulPaint::entryType() const {
  return performance_entry_names::kLargestContentfulPaint;
}

PerformanceEntryType LargestContentfulPaint::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kLargestContentfulPaint;
}

void LargestContentfulPaint::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("size", size_);
}

void LargestContentfulPaint::Trace(blink::Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
