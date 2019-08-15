// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/badging/badge.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

const char Badge::kSupplementName[] = "Badge";

Badge::~Badge() = default;

// static
Badge* Badge::From(ExecutionContext* context) {
  Badge* supplement = Supplement<ExecutionContext>::From<Badge>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<Badge>(context);
    ProvideTo(*context, supplement);
  }
  return supplement;
}

// static
void Badge::set(ScriptState* script_state, ExceptionState& exception_state) {
  BadgeFromState(script_state)->SetFlag();
}

// static
void Badge::set(ScriptState* script_state,
                uint64_t content,
                ExceptionState& exception_state) {
  if (content == 0)
    BadgeFromState(script_state)->Clear();
  else
    BadgeFromState(script_state)->SetInteger(content);
}

// static
void Badge::clear(ScriptState* script_state) {
  BadgeFromState(script_state)->Clear();
}

void Badge::SetInteger(uint64_t content) {
  badge_service_->SetInteger(content);
}

void Badge::SetFlag() {
  badge_service_->SetFlag();
}

void Badge::Clear() {
  badge_service_->ClearBadge();
}

void Badge::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

Badge::Badge(ExecutionContext* context) {
  context->GetInterfaceProvider()->GetInterface(
      badge_service_.BindNewPipeAndPassReceiver());
  DCHECK(badge_service_);
}

// static
Badge* Badge::BadgeFromState(ScriptState* script_state) {
  return Badge::From(ExecutionContext::From(script_state));
}

}  // namespace blink
