// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_STATE_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_STATE_RECORD_H_

#include "base/gtest_prod_util.h"
#include "services/device/public/mojom/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

// https://w3c.github.io/wake-lock/#concepts-and-state-record
// Per-document and per-wake lock type internal data.
class MODULES_EXPORT WakeLockStateRecord
    : public GarbageCollectedFinalized<WakeLockStateRecord> {
 public:
  WakeLockStateRecord(ExecutionContext*, WakeLockType);

  void AcquireWakeLock(ScriptPromiseResolver*);
  void ReleaseWakeLock(ScriptPromiseResolver*);
  void ClearWakeLocks();

  void Trace(blink::Visitor* visitor);

 private:
  using ActiveLocksType = HeapHashSet<Member<ScriptPromiseResolver>>;

  friend class WakeLockControllerTest;

  // Handle connection errors from |wake_lock_|.
  void OnWakeLockConnectionError();

  // A list of Promises representing active wake locks associated with the
  // responsible document.
  ActiveLocksType active_locks_;

  // An actual platform WakeLock. If bound, it means there is an active wake
  // lock for a given type.
  device::mojom::blink::WakeLockPtr wake_lock_;
  device::mojom::blink::WakeLockType wake_lock_type_;

  // ExecutionContext from which we will connect to |wake_lock_service_|.
  Member<ExecutionContext> execution_context_;

  FRIEND_TEST_ALL_PREFIXES(WakeLockStateRecordTest, AcquireWakeLock);
  FRIEND_TEST_ALL_PREFIXES(WakeLockStateRecordTest, ReleaseAllWakeLocks);
  FRIEND_TEST_ALL_PREFIXES(WakeLockStateRecordTest, ReleaseNonExistentWakeLock);
  FRIEND_TEST_ALL_PREFIXES(WakeLockStateRecordTest, ReleaseOneWakeLock);
  FRIEND_TEST_ALL_PREFIXES(WakeLockStateRecordTest, ReleaseRejectsPromise);
  FRIEND_TEST_ALL_PREFIXES(WakeLockStateRecordTest, ClearWakeLocks);
  FRIEND_TEST_ALL_PREFIXES(WakeLockStateRecordTest, WakeLockConnectionError);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_STATE_RECORD_H_
