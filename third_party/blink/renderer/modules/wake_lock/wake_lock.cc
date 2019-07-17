// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_controller.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_request_options.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
ScriptPromise WakeLock::requestPermission(ScriptState* script_state,
                                          const String& type) {
  // https://w3c.github.io/wake-lock/#requestpermission-static-method
  // 1. Let promise be a new promise.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // 2. Return promise and run the following steps in parallel:
  // 2.1. Let state be the result of running and waiting for the obtain
  //      permission steps with type.
  // 2.2. Resolve promise with state.
  auto* document = To<Document>(ExecutionContext::From(script_state));
  WakeLockController::From(document).RequestPermission(ToWakeLockType(type),
                                                       resolver);
  return promise;
}

// static
ScriptPromise WakeLock::request(ScriptState* script_state,
                                const String& type,
                                WakeLockRequestOptions* options) {
  // https://w3c.github.io/wake-lock/#request-static-method
  auto* context = ExecutionContext::From(script_state);
  DCHECK(context->IsDocument() || context->IsDedicatedWorkerGlobalScope());

  // 2.1. If document is not allowed to use the policy-controlled feature named
  //      "wake-lock", reject promise with a "NotAllowedError" DOMException and
  //      return promise.
  // [N.B. Per https://github.com/w3c/webappsec-feature-policy/issues/207 there
  // is no official support for workers in the Feature Policy spec, but we can
  // perform FP checks in workers in Blink]
  // 2.2. If the user agent denies the wake lock of this type for document,
  //      reject promise with a "NotAllowedError" DOMException and return
  //      promise.
  if (!context->GetSecurityContext().IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kWakeLock,
          ReportOptions::kReportOnFailure)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "Access to WakeLock features is disallowed by feature policy"));
  }

  if (context->IsDedicatedWorkerGlobalScope()) {
    // 3. If the current global object is the DedicatedWorkerGlobalScope object:
    // 3.1. If the current global object's owner set is empty, reject promise
    //      with a "NotAllowedError" DOMException and return promise.
    // 3.2. If type is "screen", reject promise with a "NotAllowedError"
    //      DOMException, and return promise.
    if (type == "screen") {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kNotAllowedError,
                            "Screen locks cannot be requested from workers"));
    }
  } else if (context->IsDocument()) {
    // 2. Let document be the responsible document of the current settings
    // object.
    auto* document = To<Document>(context);

    // 4. Otherwise, if the current global object is the Window object:
    // 4.1. If the document's browsing context is null, reject promise with a
    //      "NotAllowedError" DOMException and return promise.
    if (!document) {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kNotAllowedError,
                            "The document has no associated browsing context"));
    }

    // 4.2. If document is not fully active, reject promise with a
    //      "NotAllowedError" DOMException, and return promise.
    if (!document->IsActive()) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                             "The document is not active"));
    }
    // 4.3. If type is "screen" and the Document of the top-level browsing
    //      context is hidden, reject promise with a "NotAllowedError"
    //      DOMException, and return promise.
    if (type == "screen" &&
        !(document->GetPage() && document->GetPage()->IsPageVisible())) {
      return ScriptPromise::RejectWithDOMException(
          script_state, MakeGarbageCollected<DOMException>(
                            DOMExceptionCode::kNotAllowedError,
                            "The requesting page is not visible"));
    }
  }

  // 5. If options' signal member is present, then run the following steps:
  // 5.1. Let signal be options's signal member.
  // 5.2. If signal’s aborted flag is set, then reject promise with an
  //      "AbortError" DOMException and return promise.
  if (options->hasSignal() && options->signal()->aborted()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  WakeLockType wake_lock_type = ToWakeLockType(type);
  WakeLockController& controller = WakeLockController::From(context);

  // 5.3. Otherwise, add to signal:
  // 5.3.1. Run release a wake lock with promise and type.
  if (options->hasSignal()) {
    options->signal()->AddAlgorithm(WTF::Bind(
        &WakeLockController::ReleaseWakeLock, WrapWeakPersistent(&controller),
        wake_lock_type, WrapPersistent(resolver)));
  }

  // 6. Run the following steps in parallel, but abort when options' signal
  // member is present and its aborted flag is set:
  controller.RequestWakeLock(wake_lock_type, resolver, options->signal());

  // 7. Return promise.
  return promise;
}

}  // namespace blink
