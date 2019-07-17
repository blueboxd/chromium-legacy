// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_state_record.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

WakeLockController::WakeLockController(Document& document)
    : Supplement<ExecutionContext>(document),
      ContextLifecycleObserver(&document),
      PageVisibilityObserver(document.GetPage()),
      state_records_{
          MakeGarbageCollected<WakeLockStateRecord>(&document,
                                                    WakeLockType::kScreen),
          MakeGarbageCollected<WakeLockStateRecord>(&document,
                                                    WakeLockType::kSystem)} {}

WakeLockController::WakeLockController(DedicatedWorkerGlobalScope& worker_scope)
    : Supplement<ExecutionContext>(worker_scope),
      ContextLifecycleObserver(&worker_scope),
      PageVisibilityObserver(nullptr),
      state_records_{
          MakeGarbageCollected<WakeLockStateRecord>(&worker_scope,
                                                    WakeLockType::kScreen),
          MakeGarbageCollected<WakeLockStateRecord>(&worker_scope,
                                                    WakeLockType::kSystem)} {}

const char WakeLockController::kSupplementName[] = "WakeLockController";

// static
WakeLockController& WakeLockController::From(
    ExecutionContext* execution_context) {
  DCHECK(execution_context->IsDocument() ||
         execution_context->IsDedicatedWorkerGlobalScope());
  auto* controller =
      Supplement<ExecutionContext>::From<WakeLockController>(execution_context);
  if (!controller) {
    if (execution_context->IsDocument()) {
      controller = MakeGarbageCollected<WakeLockController>(
          *To<Document>(execution_context));
    } else {
      controller = MakeGarbageCollected<WakeLockController>(
          *To<DedicatedWorkerGlobalScope>(execution_context));
    }
    Supplement<ExecutionContext>::ProvideTo(*execution_context, controller);
  }
  return *controller;
}

void WakeLockController::Trace(Visitor* visitor) {
  for (WakeLockStateRecord* state_record : state_records_)
    visitor->Trace(state_record);
  PageVisibilityObserver::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

void WakeLockController::RequestWakeLock(WakeLockType type,
                                         ScriptPromiseResolver* resolver,
                                         AbortSignal* signal) {
  // https://w3c.github.io/wake-lock/#request-static-method
  // 6. [...] abort when options' signal member is present and its aborted flag
  // is set:
  DCHECK(resolver);
  // We were called via WakeLock::request(), which should have handled the
  // signal->aborted() case.
  DCHECK(!signal || !signal->aborted());
  // 6.1. Let state be the result of awaiting obtain permission steps with
  // type:
  ObtainPermission(
      type, WTF::Bind(&WakeLockController::DidReceivePermissionResponse,
                      WrapPersistent(this), type, WrapPersistent(resolver),
                      WrapPersistent(signal)));
}

void WakeLockController::DidReceivePermissionResponse(
    WakeLockType type,
    ScriptPromiseResolver* resolver,
    AbortSignal* signal,
    PermissionStatus status) {
  // https://w3c.github.io/wake-lock/#request-static-method
  DCHECK(status == PermissionStatus::GRANTED ||
         status == PermissionStatus::DENIED);
  DCHECK(resolver);
  if (signal && signal->aborted())
    return;
  // 6.1.1. If state is "denied", then reject promise with a "NotAllowedError"
  //        DOMException, and abort these steps.
  if (status != PermissionStatus::GRANTED) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Wake Lock permission request denied"));
    return;
  }
  // https://github.com/w3c/wake-lock/issues/222: the page can become hidden
  // between RequestWakeLock() and AcquireWakeLock(), in which case we need to
  // abort early.
  if (type == WakeLockType::kScreen &&
      !(GetPage() && GetPage()->IsPageVisible())) {
    ReleaseWakeLock(type, resolver);
    return;
  }
  // 6.2. Let success be the result of awaiting acquire a wake lock with promise
  // and type:
  // 6.2.1. If success is false then reject promise with a "NotAllowedError"
  //        DOMException, and abort these steps.
  AcquireWakeLock(type, resolver);
}

void WakeLockController::ReleaseWakeLock(WakeLockType type,
                                         ScriptPromiseResolver* resolver) {
  DCHECK_LE(type, WakeLockType::kMaxValue);
  WakeLockStateRecord* state_record = state_records_[static_cast<size_t>(type)];
  DCHECK(state_record);
  state_record->ReleaseWakeLock(resolver);
}

void WakeLockController::RequestPermission(WakeLockType type,
                                           ScriptPromiseResolver* resolver) {
  // https://w3c.github.io/wake-lock/#requestpermission-static-method
  // 2.1. Let state be the result of running and waiting for the obtain
  //      permission steps with type.
  // 2.2. Resolve promise with state.
  auto permission_callback = WTF::Bind(
      [](ScriptPromiseResolver* resolver, PermissionStatus status) {
        DCHECK(status == PermissionStatus::GRANTED ||
               status == PermissionStatus::DENIED);
        DCHECK(resolver);
        resolver->Resolve(PermissionStatusToString(status));
      },
      WrapPersistent(resolver));
  ObtainPermission(type, std::move(permission_callback));
}

void WakeLockController::ContextDestroyed(ExecutionContext*) {
  // https://w3c.github.io/wake-lock/#handling-document-loss-of-full-activity
  // 1. Let document be the responsible document of the current settings object.
  // 2. Let screenRecord be the platform wake lock's state record associated
  // with document and wake lock type "screen".
  // 3. For each lockPromise in screenRecord.[[WakeLockStateRecord]]:
  // 3.1. Run release a wake lock with lockPromise and "screen".
  // 4. Let systemRecord be the platform wake lock's state record associated
  // with document and wake lock type "system".
  // 5. For each lockPromise in systemRecord.[[WakeLockStateRecord]]:
  // 5.1. Run release a wake lock with lockPromise and "system".
  for (WakeLockStateRecord* state_record : state_records_) {
    if (state_record)
      state_record->ClearWakeLocks();
  }
}

void WakeLockController::PageVisibilityChanged() {
  // https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
  // 1. Let document be the Document of the top-level browsing context.
  // 2. If document's visibility state is "visible", abort these steps.
  if (GetPage() && GetPage()->IsPageVisible())
    return;
  // 3. Let screenRecord be the platform wake lock's state record associated
  // with wake lock type "screen".
  // 4. For each lockPromise in screenRecord.[[WakeLockStateRecord]]:
  // 4.1. Run release a wake lock with lockPromise and "screen".
  WakeLockStateRecord* state_record =
      state_records_[static_cast<size_t>(WakeLockType::kScreen)];
  if (state_record)
    state_record->ClearWakeLocks();
}

void WakeLockController::AcquireWakeLock(WakeLockType type,
                                         ScriptPromiseResolver* resolver) {
  DCHECK_LE(type, WakeLockType::kMaxValue);
  WakeLockStateRecord* state_record = state_records_[static_cast<size_t>(type)];
  DCHECK(state_record);
  state_record->AcquireWakeLock(resolver);
}

void WakeLockController::ObtainPermission(
    WakeLockType type,
    base::OnceCallback<void(PermissionStatus)> callback) {
  // https:w3c.github.io/wake-lock/#dfn-obtain-permission
  // Note we actually implement a simplified version of the "obtain permission"
  // algorithm that essentially just calls the "request permission to use"
  // algorithm from the Permissions spec (i.e. we bypass all the steps covering
  // calling the "query a permission" algorithm and handling its result).
  // * Right now, we can do that because there is no way for Chromium's
  //   permission system to get to the "prompt" state given how
  //   WakeLockPermissionContext is currently implemented.
  // * Even if WakeLockPermissionContext changes in the future, this Blink
  //   implementation is unlikely to change because
  //   WakeLockPermissionContext::RequestPermission() will take its
  //   |user_gesture| argument into account to actually implement a slightly
  //   altered version of "request permission to use", the behavior of which
  //   will match the definition of "obtain permission" in the Wake Lock spec.
  DCHECK(type == WakeLockType::kScreen || type == WakeLockType::kSystem);
  static_assert(
      static_cast<mojom::blink::WakeLockType>(WakeLockType::kScreen) ==
          mojom::blink::WakeLockType::kScreen,
      "WakeLockType and mojom::blink::WakeLockType must have identical values");
  static_assert(
      static_cast<mojom::blink::WakeLockType>(WakeLockType::kSystem) ==
          mojom::blink::WakeLockType::kSystem,
      "WakeLockType and mojom::blink::WakeLockType must have identical values");

  auto* local_frame = GetExecutionContext()->IsDocument()
                          ? To<Document>(GetExecutionContext())->GetFrame()
                          : nullptr;

  GetPermissionService().RequestPermission(
      CreateWakeLockPermissionDescriptor(
          static_cast<mojom::blink::WakeLockType>(type)),
      LocalFrame::HasTransientUserActivation(local_frame), std::move(callback));
}

PermissionService& WakeLockController::GetPermissionService() {
  if (!permission_service_) {
    ConnectToPermissionService(GetExecutionContext(),
                               mojo::MakeRequest(&permission_service_));
  }
  return *permission_service_;
}

}  // namespace blink
