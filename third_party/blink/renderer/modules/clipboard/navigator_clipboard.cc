// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/navigator_clipboard.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

namespace blink {

// static
const char NavigatorClipboard::kSupplementName[] = "NavigatorClipboard";

Clipboard* NavigatorClipboard::clipboard(ScriptState* script_state,
                                         Navigator& navigator) {
  NavigatorClipboard* supplement =
      Supplement<Navigator>::From<NavigatorClipboard>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorClipboard>(navigator);
    ProvideTo(navigator, supplement);
  }

  if (!supplement->GetSupplementable()->GetFrame())
    return nullptr;

  return supplement->clipboard_;
}

void NavigatorClipboard::Trace(Visitor* visitor) {
  visitor->Trace(clipboard_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorClipboard::NavigatorClipboard(Navigator& navigator)
    : Supplement<Navigator>(navigator) {
  // TODO(crbug.com/1028591): Figure out how navigator.clipboard is supposed to
  // behave in a detached execution context.
  if (!GetSupplementable()->GetFrame())
    return;

  clipboard_ = MakeGarbageCollected<Clipboard>(
      GetSupplementable()->GetFrame()->GetDocument()->ToExecutionContext());
}

}  // namespace blink
