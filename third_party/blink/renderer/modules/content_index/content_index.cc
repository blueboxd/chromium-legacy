// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_index.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ContentIndex::ContentIndex() = default;
ContentIndex::~ContentIndex() = default;

ScriptPromise ContentIndex::add(ScriptState* script_state,
                                const ContentDescription* description) {
  return ScriptPromise::RejectWithDOMException(
      script_state, MakeGarbageCollected<DOMException>(
                        DOMExceptionCode::kNotSupportedError,
                        "ContentIndex::add is not yet implemented"));
}

ScriptPromise ContentIndex::deleteDescription(ScriptState* script_state,
                                              const String& id) {
  return ScriptPromise::RejectWithDOMException(
      script_state, MakeGarbageCollected<DOMException>(
                        DOMExceptionCode::kNotSupportedError,
                        "ContentIndex::delete is not yet implemented"));
}

ScriptPromise ContentIndex::getDescriptions(ScriptState* script_state) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "ContentIndex::getDescriptions is not yet implemented"));
}

}  // namespace blink
