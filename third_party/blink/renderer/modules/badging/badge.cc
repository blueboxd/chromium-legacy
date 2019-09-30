// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/badging/badge.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/badging/badge_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

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
  Badge::set(script_state, BadgeOptions::Create(), exception_state);
}

// static
void Badge::set(ScriptState* script_state,
                const BadgeOptions* options,
                ExceptionState& exception_state) {
  BadgeFromState(script_state)
      ->SetBadge(options->scope(), mojom::blink::BadgeValue::NewFlag(0),
                 exception_state);
}

// static
void Badge::set(ScriptState* script_state,
                uint64_t content,
                ExceptionState& exception_state) {
  Badge::set(script_state, content, BadgeOptions::Create(), exception_state);
}

// static
void Badge::set(ScriptState* script_state,
                uint64_t content,
                const BadgeOptions* options,
                ExceptionState& exception_state) {
  if (content == 0) {
    Badge::clear(script_state, options, exception_state);
  } else {
    BadgeFromState(script_state)
        ->SetBadge(options->scope(),
                   mojom::blink::BadgeValue::NewNumber(content),
                   exception_state);
  }
}

// static
void Badge::clear(ScriptState* script_state,
                  const BadgeOptions* options,
                  ExceptionState& exception_state) {
  BadgeFromState(script_state)->ClearBadge(options->scope(), exception_state);
}

void Badge::SetBadge(WTF::String scope,
                     mojom::blink::BadgeValuePtr value,
                     ExceptionState& exception_state) {
  base::Optional<KURL> scope_url = ScopeStringToURL(scope, exception_state);
  if (!scope_url)
    return;

  badge_service_->SetBadge(std::move(value));
}

void Badge::ClearBadge(WTF::String scope, ExceptionState& exception_state) {
  base::Optional<KURL> scope_url = ScopeStringToURL(scope, exception_state);
  if (!scope_url)
    return;

  badge_service_->ClearBadge();
}

void Badge::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
  ScriptWrappable::Trace(visitor);

  visitor->Trace(execution_context_);
}

Badge::Badge(ExecutionContext* context) : execution_context_(context) {
  context->GetBrowserInterfaceBroker().GetInterface(
      badge_service_.BindNewPipeAndPassReceiver());
  DCHECK(badge_service_);
}

// static
Badge* Badge::BadgeFromState(ScriptState* script_state) {
  return Badge::From(ExecutionContext::From(script_state));
}

base::Optional<KURL> Badge::ScopeStringToURL(WTF::String& scope,
                                             ExceptionState& exception_state) {
  // Resolve |scope| against the URL of the current document/worker.
  KURL scope_url = KURL(execution_context_->Url(), scope);

  if (!scope_url.IsValid()) {
    exception_state.ThrowTypeError("Invalid scope URL");
    return base::nullopt;
  }

  // Same-origin check. Note that this is also checked in BadgeManager on the
  // browser side for security, but it is also checked here to report the error.
  scoped_refptr<SecurityOrigin> scope_origin =
      SecurityOrigin::Create(scope_url);
  if (!execution_context_->GetSecurityOrigin()->IsSameSchemeHostPort(
          scope_origin.get())) {
    exception_state.ThrowSecurityError(
        "Scope URL is not in the current origin");
    return base::nullopt;
  }

  return base::Optional<KURL>(scope_url);
}

}  // namespace blink
