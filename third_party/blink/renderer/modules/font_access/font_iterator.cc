// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_iterator.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_font_iterator_entry.h"
#include "third_party/blink/renderer/modules/font_access/font_metadata.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FontIterator::FontIterator(const std::vector<FontEnumerationEntry>& entries) {
  for (auto entry : entries) {
    entries_.push_back(FontMetadata::Create(entry));
  }
}

ScriptPromise FontIterator::next(ScriptState* script_state) {
  if (!entries_.IsEmpty()) {
    FontMetadata* entry = entries_.TakeFirst();
    auto* result = FontIteratorEntry::Create();
    result->setValue(entry);
    return ScriptPromise::Cast(script_state, ToV8(result, script_state));
  }

  auto* result = FontIteratorEntry::Create();
  result->setDone(true);
  return ScriptPromise::Cast(script_state, ToV8(result, script_state));
}

void FontIterator::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(entries_);
}

}  // namespace blink
