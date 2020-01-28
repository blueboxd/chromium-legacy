// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "third_party/blink/renderer/modules/badging/navigator_badge.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"

namespace blink {

const char NavigatorBadge::kSupplementName[] = "NavigatorBadge";

// static
NavigatorBadge& NavigatorBadge::From(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  NavigatorBadge* supplement =
      Supplement<ExecutionContext>::From<NavigatorBadge>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorBadge>(context);
    ProvideTo(*context, supplement);
  }
  return *supplement;
}

NavigatorBadge::NavigatorBadge(ExecutionContext* context) {
  context->GetBrowserInterfaceBroker().GetInterface(
      badge_service_.BindNewPipeAndPassReceiver());
  DCHECK(badge_service_);
}

// static
ScriptPromise NavigatorBadge::setAppBadge(ScriptState* script_state,
                                          Navigator& /*navigator*/) {
  return SetAppBadgeHelper(script_state, mojom::blink::BadgeValue::NewFlag(0));
}

// static
ScriptPromise NavigatorBadge::setAppBadge(ScriptState* script_state,
                                          WorkerNavigator& /*navigator*/) {
  return SetAppBadgeHelper(script_state, mojom::blink::BadgeValue::NewFlag(0));
}

// static
ScriptPromise NavigatorBadge::setAppBadge(ScriptState* script_state,
                                          Navigator& /*navigator*/,
                                          uint64_t content) {
  return SetAppBadgeHelper(script_state,
                           mojom::blink::BadgeValue::NewNumber(content));
}

// static
ScriptPromise NavigatorBadge::setAppBadge(ScriptState* script_state,
                                          WorkerNavigator& /*navigator*/,
                                          uint64_t content) {
  return SetAppBadgeHelper(script_state,
                           mojom::blink::BadgeValue::NewNumber(content));
}

// static
ScriptPromise NavigatorBadge::clearAppBadge(ScriptState* script_state,
                                            Navigator& /*navigator*/) {
  return ClearAppBadgeHelper(script_state);
}

// static
ScriptPromise NavigatorBadge::clearAppBadge(ScriptState* script_state,
                                            WorkerNavigator& /*navigator*/) {
  return ClearAppBadgeHelper(script_state);
}

void NavigatorBadge::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
}

// static
ScriptPromise NavigatorBadge::SetAppBadgeHelper(
    ScriptState* script_state,
    mojom::blink::BadgeValuePtr badge_value) {
  if (badge_value->is_number() && badge_value->get_number() == 0)
    return ClearAppBadgeHelper(script_state);

  From(script_state).badge_service_->SetBadge(std::move(badge_value));
  return ScriptPromise::CastUndefined(script_state);
}

// static
ScriptPromise NavigatorBadge::ClearAppBadgeHelper(ScriptState* script_state) {
  From(script_state).badge_service_->ClearBadge();
  return ScriptPromise::CastUndefined(script_state);
}

}  // namespace blink
