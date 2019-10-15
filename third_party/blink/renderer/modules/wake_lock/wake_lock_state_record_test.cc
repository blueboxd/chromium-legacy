// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_state_record.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

WakeLockStateRecord* MakeStateRecord(WakeLockTestingContext& context,
                                     WakeLockType type) {
  return MakeGarbageCollected<WakeLockStateRecord>(context.GetDocument(), type);
}

}  // namespace

TEST(WakeLockStateRecordTest, AcquireWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* state_record = MakeStateRecord(context, WakeLockType::kScreen);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(state_record->wake_lock_.is_bound());

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise2 = resolver2->Promise();

  state_record->AcquireWakeLock(resolver1);
  state_record->AcquireWakeLock(resolver2);
  screen_lock.WaitForRequest();

  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  auto* sentinel1 =
      ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(promise1);
  auto* sentinel2 =
      ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(promise2);

  EXPECT_TRUE(state_record->wake_lock_sentinels_.Contains(sentinel1));
  EXPECT_TRUE(state_record->wake_lock_sentinels_.Contains(sentinel2));
  EXPECT_EQ(2U, state_record->wake_lock_sentinels_.size());
  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_TRUE(state_record->wake_lock_.is_bound());
}

TEST(WakeLockStateRecordTest, ReleaseAllWakeLocks) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* state_record = MakeStateRecord(context, WakeLockType::kScreen);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise = resolver->Promise();

  state_record->AcquireWakeLock(resolver);
  screen_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(promise);

  EXPECT_EQ(1U, state_record->wake_lock_sentinels_.size());
  EXPECT_TRUE(screen_lock.is_acquired());

  auto* sentinel =
      ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(promise);

  state_record->UnregisterSentinel(sentinel);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(0U, state_record->wake_lock_sentinels_.size());
  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(state_record->wake_lock_.is_bound());
}

TEST(WakeLockStateRecordTest, ReleaseOneWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* state_record = MakeStateRecord(context, WakeLockType::kScreen);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise2 = resolver2->Promise();

  state_record->AcquireWakeLock(resolver1);
  state_record->AcquireWakeLock(resolver2);
  screen_lock.WaitForRequest();

  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_EQ(2U, state_record->wake_lock_sentinels_.size());

  auto* sentinel1 =
      ScriptPromiseUtils::GetPromiseResolutionAsWakeLockSentinel(promise1);
  EXPECT_TRUE(state_record->wake_lock_sentinels_.Contains(sentinel1));

  state_record->UnregisterSentinel(sentinel1);
  EXPECT_FALSE(state_record->wake_lock_sentinels_.Contains(sentinel1));
  EXPECT_TRUE(state_record->wake_lock_.is_bound());
  EXPECT_EQ(1U, state_record->wake_lock_sentinels_.size());
  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockStateRecordTest, ClearEmptyWakeLockSentinelList) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* state_record = MakeStateRecord(context, WakeLockType::kSystem);

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  EXPECT_FALSE(system_lock.is_acquired());

  state_record->ClearWakeLocks();
  test::RunPendingTasks();

  EXPECT_FALSE(system_lock.is_acquired());
}

TEST(WakeLockStateRecordTest, ClearWakeLocks) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* state_record = MakeStateRecord(context, WakeLockType::kSystem);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise2 = resolver2->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);

  state_record->AcquireWakeLock(resolver1);
  state_record->AcquireWakeLock(resolver2);
  system_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  EXPECT_EQ(2U, state_record->wake_lock_sentinels_.size());

  state_record->ClearWakeLocks();
  system_lock.WaitForCancelation();

  EXPECT_EQ(0U, state_record->wake_lock_sentinels_.size());
  EXPECT_FALSE(system_lock.is_acquired());
}

TEST(WakeLockStateRecordTest, WakeLockConnectionError) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* state_record = MakeStateRecord(context, WakeLockType::kSystem);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise2 = resolver2->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);

  state_record->AcquireWakeLock(resolver1);
  state_record->AcquireWakeLock(resolver2);
  system_lock.WaitForRequest();
  context.WaitForPromiseFulfillment(promise1);
  context.WaitForPromiseFulfillment(promise2);

  EXPECT_EQ(2U, state_record->wake_lock_sentinels_.size());

  // Unbind and wait for the disconnection to reach |wake_lock_|'s
  // disconnection handler.
  system_lock.Unbind();
  state_record->wake_lock_.FlushForTesting();

  EXPECT_EQ(0U, state_record->wake_lock_sentinels_.size());
  EXPECT_FALSE(state_record->wake_lock_);
  EXPECT_FALSE(system_lock.is_acquired());
}

}  // namespace blink
