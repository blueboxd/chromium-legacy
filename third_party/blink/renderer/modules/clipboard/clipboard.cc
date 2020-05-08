// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include <utility>
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item_options.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

namespace blink {

Clipboard::Clipboard(ExecutionContext* context)
    : ExecutionContextClient(context) {
  DCHECK(context);
}

ScriptPromise Clipboard::read(ScriptState* script_state) {
  return read(script_state, ClipboardItemOptions::Create());
}

ScriptPromise Clipboard::read(ScriptState* script_state,
                              ClipboardItemOptions* options) {
  return ClipboardPromise::CreateForRead(GetExecutionContext(), script_state,
                                         options);
}

ScriptPromise Clipboard::readText(ScriptState* script_state) {
  return ClipboardPromise::CreateForReadText(GetExecutionContext(),
                                             script_state);
}

ScriptPromise Clipboard::write(ScriptState* script_state,
                               const HeapVector<Member<ClipboardItem>>& data) {
  return ClipboardPromise::CreateForWrite(GetExecutionContext(), script_state,
                                          std::move(data));
}

ScriptPromise Clipboard::writeText(ScriptState* script_state,
                                   const String& data) {
  return ClipboardPromise::CreateForWriteText(GetExecutionContext(),
                                              script_state, data);
}

const AtomicString& Clipboard::InterfaceName() const {
  return event_target_names::kClipboard;
}

ExecutionContext* Clipboard::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void Clipboard::Trace(Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
