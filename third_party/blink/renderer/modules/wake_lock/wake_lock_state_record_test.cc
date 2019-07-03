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

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise1 = resolver1->Promise();
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise2 = resolver2->Promise();

  state_record->AcquireWakeLock(resolver1);
  state_record->AcquireWakeLock(resolver2);
  screen_lock.WaitForRequest();

  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise1));
  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise2));
  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_EQ(2U, state_record->active_locks_.size());
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

  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise));
  EXPECT_EQ(1U, state_record->active_locks_.size());
  EXPECT_TRUE(screen_lock.is_acquired());

  state_record->ReleaseWakeLock(resolver);
  context.WaitForPromiseRejection(promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise));
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(promise);
  ASSERT_NE(nullptr, dom_exception);
  EXPECT_EQ("AbortError", dom_exception->name());

  EXPECT_EQ(0U, state_record->active_locks_.size());
  EXPECT_FALSE(screen_lock.is_acquired());
}

// Test that trying to remove an entry that does not exist is a no-op. This can
// happen, for example, when a page visibility change releases a screen lock
// and a call to AbortController.abort() causes an attempt to release the same
// lock again.
TEST(WakeLockStateRecordTest, ReleaseNonExistentWakeLock) {
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
  state_record->ReleaseWakeLock(resolver);
  context.WaitForPromiseRejection(promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise));
  EXPECT_EQ(0U, state_record->active_locks_.size());
  EXPECT_FALSE(screen_lock.is_acquired());

  state_record->ReleaseWakeLock(resolver);
  test::RunPendingTasks();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise));
  EXPECT_EQ(0U, state_record->active_locks_.size());
  EXPECT_FALSE(screen_lock.is_acquired());
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

  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise1));
  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise2));
  EXPECT_EQ(2U, state_record->active_locks_.size());
  EXPECT_TRUE(screen_lock.is_acquired());

  state_record->ReleaseWakeLock(resolver1);
  context.WaitForPromiseRejection(promise1);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise1));
  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise2));
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(promise1);
  ASSERT_NE(nullptr, dom_exception);
  EXPECT_EQ("AbortError", dom_exception->name());

  EXPECT_EQ(1U, state_record->active_locks_.size());
  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockStateRecordTest, ReleaseRejectsPromise) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto* state_record = MakeStateRecord(context, WakeLockType::kScreen);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise = resolver->Promise();

  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise));

  state_record->ReleaseWakeLock(resolver);
  context.WaitForPromiseRejection(promise);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise));
  EXPECT_EQ(0U, state_record->active_locks_.size());
  EXPECT_FALSE(screen_lock.is_acquired());
}

TEST(WakeLockStateRecordTest, ClearEmptyActiveLocksList) {
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

  state_record->ClearWakeLocks();
  context.WaitForPromiseRejection(promise1);
  context.WaitForPromiseRejection(promise2);
  system_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise1));
  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise2));
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(promise1);
  ASSERT_NE(nullptr, dom_exception);
  EXPECT_EQ("AbortError", dom_exception->name());
  dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(promise2);
  ASSERT_NE(nullptr, dom_exception);
  EXPECT_EQ("AbortError", dom_exception->name());

  EXPECT_EQ(0U, state_record->active_locks_.size());
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

  system_lock.Unbind();
  context.WaitForPromiseRejection(promise1);
  context.WaitForPromiseRejection(promise2);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise1));
  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise2));
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(promise1);
  ASSERT_NE(nullptr, dom_exception);
  EXPECT_EQ("AbortError", dom_exception->name());
  dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(promise2);
  ASSERT_NE(nullptr, dom_exception);
  EXPECT_EQ("AbortError", dom_exception->name());

  EXPECT_EQ(0U, state_record->active_locks_.size());
  EXPECT_FALSE(state_record->wake_lock_);
  EXPECT_FALSE(system_lock.is_acquired());
}

}  // namespace blink
