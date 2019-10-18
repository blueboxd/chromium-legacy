// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"

#include "base/logging.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WakeLockManager::WakeLockManager(ExecutionContext* execution_context,
                                 WakeLockType type)
    : wake_lock_type_(type), execution_context_(execution_context) {
  DCHECK_NE(execution_context, nullptr);
}

void WakeLockManager::AcquireWakeLock(ScriptPromiseResolver* resolver) {
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
  // 4.3. Add lockPromise to record.[[ActiveLocks]].
  // 5. Return active.
  if (!wake_lock_) {
    mojo::Remote<mojom::blink::WakeLockService> wake_lock_service;
    execution_context_->GetBrowserInterfaceBroker().GetInterface(
        wake_lock_service.BindNewPipeAndPassReceiver());

    wake_lock_service->GetWakeLock(ToMojomWakeLockType(wake_lock_type_),
                                   device::mojom::blink::WakeLockReason::kOther,
                                   "Blink Wake Lock",
                                   wake_lock_.BindNewPipeAndPassReceiver());
    wake_lock_.set_disconnect_handler(WTF::Bind(
        &WakeLockManager::OnWakeLockConnectionError, WrapWeakPersistent(this)));
    wake_lock_->RequestWakeLock();
  }
  auto* sentinel = MakeGarbageCollected<WakeLockSentinel>(
      resolver->GetScriptState(), wake_lock_type_, this);
  wake_lock_sentinels_.insert(sentinel);
  resolver->Resolve(sentinel);
}

void WakeLockManager::UnregisterSentinel(WakeLockSentinel* sentinel) {
  auto iterator = wake_lock_sentinels_.find(sentinel);
  DCHECK(iterator != wake_lock_sentinels_.end());
  wake_lock_sentinels_.erase(iterator);

  if (wake_lock_sentinels_.IsEmpty() && wake_lock_.is_bound()) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();
  }
}

void WakeLockManager::ClearWakeLocks() {
  while (!wake_lock_sentinels_.IsEmpty())
    (*wake_lock_sentinels_.begin())->DoRelease();
}

void WakeLockManager::OnWakeLockConnectionError() {
  wake_lock_.reset();
  ClearWakeLocks();
}

void WakeLockManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(wake_lock_sentinels_);
}

}  // namespace blink
