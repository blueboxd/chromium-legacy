// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_CONTROLLER_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AbortSignal;
class ExecutionContext;
class ScriptPromiseResolver;
class WakeLockStateRecord;

// WakeLockController is used to track per-Document wake lock state and react to
// Document changes appropriately.
class MODULES_EXPORT WakeLockController final
    : public GarbageCollectedFinalized<WakeLockController>,
      public Supplement<ExecutionContext>,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(WakeLockController);

 public:
  static const char kSupplementName[];

  explicit WakeLockController(Document&);
  explicit WakeLockController(DedicatedWorkerGlobalScope&);

  static WakeLockController& From(ExecutionContext*);

  void Trace(blink::Visitor*) override;

  void RequestWakeLock(WakeLockType type,
                       ScriptPromiseResolver* resolver,
                       AbortSignal* signal);

  void ReleaseWakeLock(WakeLockType type, ScriptPromiseResolver*);

  void RequestPermission(WakeLockType type, ScriptPromiseResolver*);

 private:
  // ContextLifecycleObserver implementation
  void ContextDestroyed(ExecutionContext*) override;

  // PageVisibilityObserver implementation
  void PageVisibilityChanged() override;

  void AcquireWakeLock(WakeLockType type, ScriptPromiseResolver*);

  void DidReceivePermissionResponse(WakeLockType type,
                                    ScriptPromiseResolver*,
                                    AbortSignal*,
                                    mojom::blink::PermissionStatus);

  // Permission handling
  void ObtainPermission(
      WakeLockType type,
      base::OnceCallback<void(mojom::blink::PermissionStatus)> callback);
  mojom::blink::PermissionService& GetPermissionService();

  mojom::blink::PermissionServicePtr permission_service_;

  // https://w3c.github.io/wake-lock/#concepts-and-state-record
  // Each platform wake lock (one per wake lock type) has an associated state
  // record per responsible document [...] internal slots.
  Member<WakeLockStateRecord> state_records_[kWakeLockTypeCount];

  FRIEND_TEST_ALL_PREFIXES(WakeLockControllerTest, AcquireScreenWakeLock);
  FRIEND_TEST_ALL_PREFIXES(WakeLockControllerTest, AcquireSystemWakeLock);
  FRIEND_TEST_ALL_PREFIXES(WakeLockControllerTest, AcquireMultipleLocks);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_CONTROLLER_H_
