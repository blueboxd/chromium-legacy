// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_state_record.h"

#include "base/logging.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WakeLockStateRecord::WakeLockStateRecord(ExecutionContext* execution_context,
                                         WakeLockType type)
    : wake_lock_type_(ToMojomWakeLockType(type)),
      execution_context_(execution_context) {
  DCHECK_NE(execution_context, nullptr);
}

void WakeLockStateRecord::AcquireWakeLock(ScriptPromiseResolver* resolver) {
  // https://w3c.github.io/wake-lock/#acquire-wake-lock-algorithm
  // 1. If the wake lock for type type is not applicable, return false.
  // 2. Set active to true if the platform wake lock has an active wake lock for
  // type.
  // 3. Otherwise, ask the underlying operation system to acquire the wake lock
  // of type type and set active to true if the operation succeeded, or else
  // false.
  // 4. If active is true:
  // 4.1. Let document be the responsible document of the current settings
  //      object.
  // 4.2. Let record be the platform wake lock's state record associated with
  //      document and type.
  // 4.3. Add lockPromise to record.[[WakeLockStateRecord]].
  // 5. Return active.
  if (!wake_lock_) {
    auto* interface_provider = execution_context_->GetInterfaceProvider();
    DCHECK(interface_provider);
    mojom::blink::WakeLockServicePtr wake_lock_service;
    interface_provider->GetInterface(mojo::MakeRequest(&wake_lock_service));

    wake_lock_service->GetWakeLock(
        wake_lock_type_, device::mojom::blink::WakeLockReason::kOther,
        "Blink Wake Lock", mojo::MakeRequest(&wake_lock_));
    wake_lock_.set_connection_error_handler(
        WTF::Bind(&WakeLockStateRecord::OnWakeLockConnectionError,
                  WrapWeakPersistent(this)));
    wake_lock_->RequestWakeLock();
  }
  DCHECK(!active_locks_.Contains(resolver));
  active_locks_.insert(resolver);
}

void WakeLockStateRecord::ReleaseWakeLock(ScriptPromiseResolver* resolver) {
  // https://w3c.github.io/wake-lock/#release-wake-lock-algorithm
  // 1. Reject lockPromise with an "AbortError" DOMException.
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Wake Lock released"));

  // 2. Let document be the responsible document of the current settings object.
  // 3. Let record be the platform wake lock's state record associated with
  // document and type.
  // 4. If record.[[ActiveLocks]] does not contain lockPromise, abort these
  // steps.
  auto iterator = active_locks_.find(resolver);
  if (iterator == active_locks_.end())
    return;

  // 5. Remove lockPromise from record.[[ActiveLocks]].
  active_locks_.erase(iterator);

  // 6. If the internal slot [[ActiveLocks]] of all the platform wake lock's
  // state records are all empty, then run the following steps in parallel:
  // 6.1. Ask the underlying operation system to release the wake lock of type
  //      type and let success be true if the operation succeeded, or else
  //      false.
  if (active_locks_.IsEmpty() && wake_lock_.is_bound()) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();

    // 6.2. If success is true and type is "screen" run the following:
    // 6.2.1. Reset the platform-specific inactivity timer after which the
    //        screen is actually turned off.
  }
}

void WakeLockStateRecord::ClearWakeLocks() {
  while (!active_locks_.IsEmpty())
    ReleaseWakeLock(*active_locks_.begin());
}

void WakeLockStateRecord::OnWakeLockConnectionError() {
  wake_lock_.reset();
  ClearWakeLocks();
}

void WakeLockStateRecord::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(active_locks_);
}

}  // namespace blink
