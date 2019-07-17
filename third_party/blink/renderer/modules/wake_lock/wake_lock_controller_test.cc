// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

TEST(WakeLockControllerTest, RequestWakeLockGranted) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);

  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  controller.RequestWakeLock(WakeLockType::kScreen, screen_resolver, nullptr);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  MockPermissionService& permission_service = context.GetPermissionService();

  permission_service.WaitForPermissionRequest(WakeLockType::kScreen);
  screen_lock.WaitForRequest();

  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockControllerTest, RequestWakeLockDenied) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::DENIED);

  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  controller.RequestWakeLock(WakeLockType::kSystem, system_resolver, nullptr);

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  MockPermissionService& permission_service = context.GetPermissionService();

  permission_service.WaitForPermissionRequest(WakeLockType::kSystem);
  context.WaitForPromiseRejection(system_promise);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(system_promise));
  EXPECT_FALSE(system_lock.is_acquired());

  // System locks are not allowed by default, so the promise should have been
  // rejected with a NotAllowedError DOMException.
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(system_promise);
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("NotAllowedError", dom_exception->name());
}

// Abort early in DidReceivePermissionResponse().
TEST(WakeLockControllerTest, RequestWakeLockAbortEarly) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  auto* abort_signal = MakeGarbageCollected<AbortSignal>(context.GetDocument());
  abort_signal->AddAlgorithm(WTF::Bind(
      &WakeLockController::ReleaseWakeLock, WrapWeakPersistent(&controller),
      WakeLockType::kScreen, WrapPersistent(screen_resolver)));

  controller.RequestWakeLock(
      WakeLockType::kScreen,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()),
      abort_signal);

  MockPermissionService& permission_service = context.GetPermissionService();

  permission_service.WaitForPermissionRequest(WakeLockType::kScreen);
  abort_signal->SignalAbort();

  context.WaitForPromiseRejection(screen_promise);

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  EXPECT_FALSE(screen_lock.is_acquired());

  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(screen_promise);
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("AbortError", dom_exception->name());
}

TEST(WakeLockControllerTest, AcquireScreenWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  controller.AcquireWakeLock(
      WakeLockType::kScreen,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  screen_lock.WaitForRequest();

  EXPECT_TRUE(screen_lock.is_acquired());
}

TEST(WakeLockControllerTest, AcquireSystemWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  controller.AcquireWakeLock(
      WakeLockType::kSystem,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  system_lock.WaitForRequest();

  EXPECT_TRUE(system_lock.is_acquired());
}

TEST(WakeLockControllerTest, AcquireMultipleLocks) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  controller.AcquireWakeLock(
      WakeLockType::kScreen,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  controller.AcquireWakeLock(
      WakeLockType::kSystem,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));
  controller.AcquireWakeLock(
      WakeLockType::kSystem,
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState()));

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  screen_lock.WaitForRequest();
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  system_lock.WaitForRequest();

  EXPECT_TRUE(screen_lock.is_acquired());
  EXPECT_TRUE(system_lock.is_acquired());
}

TEST(WakeLockControllerTest, ReleaseUnaquiredWakeLockRejectsPromise) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise = resolver->Promise();

  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(promise));
  controller.ReleaseWakeLock(WakeLockType::kScreen, resolver);
  context.WaitForPromiseRejection(promise);

  // The promise is always rejected, even if it is not in [[ActiveLocks]].
  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise));
  EXPECT_FALSE(screen_lock.is_acquired());
}

TEST(WakeLockControllerTest, ReleaseWakeLock) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise promise = resolver->Promise();

  controller.RequestWakeLock(WakeLockType::kScreen, resolver,
                             /*signal=*/nullptr);
  screen_lock.WaitForRequest();
  controller.ReleaseWakeLock(WakeLockType::kScreen, resolver);
  context.WaitForPromiseRejection(promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(promise));
  EXPECT_FALSE(screen_lock.is_acquired());
}

// This is actually part of WakeLock::request(), but it is easier to just test
// it here.
TEST(WakeLockControllerTest, AbortSignal) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  auto* abort_signal = MakeGarbageCollected<AbortSignal>(context.GetDocument());
  abort_signal->AddAlgorithm(WTF::Bind(
      &WakeLockController::ReleaseWakeLock, WrapWeakPersistent(&controller),
      WakeLockType::kScreen, WrapPersistent(screen_resolver)));

  controller.RequestWakeLock(WakeLockType::kScreen, screen_resolver,
                             /*signal=*/nullptr);
  screen_lock.WaitForRequest();

  abort_signal->SignalAbort();
  context.WaitForPromiseRejection(screen_promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  EXPECT_FALSE(screen_lock.is_acquired());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-full-activity
TEST(WakeLockControllerTest, LossOfDocumentActivity) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::GRANTED);

  // First, acquire a handful of locks of different types.
  auto* screen_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise1 = screen_resolver1->Promise();
  auto* screen_resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise2 = screen_resolver2->Promise();
  auto* system_resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise1 = system_resolver1->Promise();
  controller.RequestWakeLock(WakeLockType::kScreen, screen_resolver1,
                             /*signal=*/nullptr);
  controller.RequestWakeLock(WakeLockType::kScreen, screen_resolver2,
                             /*signal=*/nullptr);
  screen_lock.WaitForRequest();
  controller.RequestWakeLock(WakeLockType::kSystem, system_resolver1,
                             /*signal=*/nullptr);
  system_lock.WaitForRequest();

  // Now shut down our Document and make sure all [[ActiveLocks]] slots have
  // been cleared. We cannot check that the promises have been rejected because
  // ScriptPromiseResolver::Reject() will bail out if we no longer have a valid
  // execution context.
  context.GetDocument()->Shutdown();
  screen_lock.WaitForCancelation();
  system_lock.WaitForCancelation();

  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_FALSE(system_lock.is_acquired());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
TEST(WakeLockControllerTest, PageVisibilityHidden) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  controller.RequestWakeLock(WakeLockType::kScreen, screen_resolver,
                             /*signal=*/nullptr);
  controller.RequestWakeLock(WakeLockType::kSystem, system_resolver,
                             /*signal=*/nullptr);

  screen_lock.WaitForRequest();
  system_lock.WaitForRequest();

  context.GetDocument()->GetPage()->SetIsHidden(true, false);

  context.WaitForPromiseRejection(screen_promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  DOMException* dom_exception =
      ScriptPromiseUtils::GetPromiseResolutionAsDOMException(screen_promise);
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ("AbortError", dom_exception->name());
  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(system_promise));
  EXPECT_TRUE(system_lock.is_acquired());

  context.GetDocument()->GetPage()->SetIsHidden(false, false);

  auto* other_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise other_promise = system_resolver->Promise();
  controller.RequestWakeLock(WakeLockType::kScreen, other_resolver,
                             /*signal=*/nullptr);
  screen_lock.WaitForRequest();
  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(other_promise));
  EXPECT_TRUE(screen_lock.is_acquired());
}

// https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
TEST(WakeLockControllerTest, PageVisibilityHiddenBeforeLockAcquisition) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);
  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  MockWakeLock& system_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kSystem);
  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  controller.RequestWakeLock(WakeLockType::kScreen, screen_resolver,
                             /*signal=*/nullptr);
  controller.RequestWakeLock(WakeLockType::kSystem, system_resolver,
                             /*signal=*/nullptr);
  context.GetDocument()->GetPage()->SetIsHidden(true, false);

  context.WaitForPromiseRejection(screen_promise);
  system_lock.WaitForRequest();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  EXPECT_FALSE(screen_lock.is_acquired());
  EXPECT_EQ(v8::Promise::kPending,
            ScriptPromiseUtils::GetPromiseState(system_promise));
  EXPECT_TRUE(system_lock.is_acquired());
}

// Check that hiding a page and signaling abort does not try to delete a
// screen lock twice.
TEST(WakeLockControllerTest, PageVisibilityAndAbortSignal) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kScreen, mojom::blink::PermissionStatus::GRANTED);

  MockWakeLock& screen_lock =
      wake_lock_service.get_wake_lock(WakeLockType::kScreen);
  auto* screen_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise screen_promise = screen_resolver->Promise();

  auto* abort_signal = MakeGarbageCollected<AbortSignal>(context.GetDocument());
  abort_signal->AddAlgorithm(WTF::Bind(
      &WakeLockController::ReleaseWakeLock, WrapWeakPersistent(&controller),
      WakeLockType::kScreen, WrapPersistent(screen_resolver)));

  controller.RequestWakeLock(WakeLockType::kScreen, screen_resolver,
                             /*signal=*/nullptr);
  screen_lock.WaitForRequest();

  context.GetDocument()->GetPage()->SetIsHidden(true, false);
  context.WaitForPromiseRejection(screen_promise);
  screen_lock.WaitForCancelation();

  EXPECT_EQ(v8::Promise::kRejected,
            ScriptPromiseUtils::GetPromiseState(screen_promise));
  EXPECT_FALSE(screen_lock.is_acquired());

  abort_signal->SignalAbort();
  test::RunPendingTasks();

  EXPECT_FALSE(screen_lock.is_acquired());
}

TEST(WakeLockControllerTest, RequestPermissionGranted) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::GRANTED);

  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  controller.RequestPermission(WakeLockType::kSystem, system_resolver);
  context.WaitForPromiseFulfillment(system_promise);

  EXPECT_EQ(v8::Promise::kFulfilled,
            ScriptPromiseUtils::GetPromiseState(system_promise));
  EXPECT_EQ("granted",
            ScriptPromiseUtils::GetPromiseResolutionAsString(system_promise));
}

TEST(WakeLockControllerTest, RequestPermissionDenied) {
  MockWakeLockService wake_lock_service;
  WakeLockTestingContext context(&wake_lock_service);
  auto& controller = WakeLockController::From(context.GetDocument());

  context.GetPermissionService().SetPermissionResponse(
      WakeLockType::kSystem, mojom::blink::PermissionStatus::DENIED);

  auto* system_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(context.GetScriptState());
  ScriptPromise system_promise = system_resolver->Promise();

  controller.RequestPermission(WakeLockType::kSystem, system_resolver);
  context.WaitForPromiseFulfillment(system_promise);

  EXPECT_EQ(v8::Promise::kFulfilled,
            ScriptPromiseUtils::GetPromiseState(system_promise));
  EXPECT_EQ("denied",
            ScriptPromiseUtils::GetPromiseResolutionAsString(system_promise));
}

}  // namespace blink
