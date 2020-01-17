// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include <utility>
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

namespace blink {

Clipboard::Clipboard(SystemClipboard* system_clipboard,
                     ExecutionContext* context)
    : ContextLifecycleObserver(context), system_clipboard_(system_clipboard) {
  DCHECK(system_clipboard);
}

ScriptPromise Clipboard::read(ScriptState* script_state) {
  return ClipboardPromise::CreateForRead(system_clipboard_, script_state);
}

ScriptPromise Clipboard::readText(ScriptState* script_state) {
  return ClipboardPromise::CreateForReadText(system_clipboard_, script_state);
}

ScriptPromise Clipboard::write(ScriptState* script_state,
                               const HeapVector<Member<ClipboardItem>>& data) {
  return ClipboardPromise::CreateForWrite(system_clipboard_, script_state,
                                          std::move(data));
}

ScriptPromise Clipboard::writeText(ScriptState* script_state,
                                   const String& data) {
  return ClipboardPromise::CreateForWriteText(system_clipboard_, script_state,
                                              data);
}

const AtomicString& Clipboard::InterfaceName() const {
  return event_target_names::kClipboard;
}

ExecutionContext* Clipboard::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void Clipboard::Trace(blink::Visitor* visitor) {
  visitor->Trace(system_clipboard_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
