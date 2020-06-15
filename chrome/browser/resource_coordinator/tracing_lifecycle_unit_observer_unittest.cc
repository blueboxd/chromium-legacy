// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tracing_lifecycle_unit_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::StrEq;
using testing::StrictMock;

namespace resource_coordinator {

namespace {

class MockTracingLifecycleUnitObserver : public TracingLifecycleUnitObserver {
 public:
  MockTracingLifecycleUnitObserver() = default;

  MOCK_METHOD2(EmitStateBeginEvent, void(const char*, const char*));
  MOCK_METHOD1(EmitStateEndEvent, void(const char*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTracingLifecycleUnitObserver);
};

}  // namespace

TEST(TracingLifecycleUnitObserverTest, Tracing) {
  constexpr LifecycleUnitStateChangeReason kReason =
      LifecycleUnitStateChangeReason::BROWSER_INITIATED;
  constexpr const char kTitle[] = "Dummy Title";

  TestLifecycleUnit lifecycle_unit;
  lifecycle_unit.SetTitle(base::UTF8ToUTF16(kTitle));

  // |observer| deletes itself when |lifecycle_unit| goes out of scope.
  MockTracingLifecycleUnitObserver* observer =
      new StrictMock<MockTracingLifecycleUnitObserver>();
  lifecycle_unit.AddObserver(observer);

  lifecycle_unit.SetState(LifecycleUnitState::ACTIVE, kReason);

  // Pending freeze.
  EXPECT_CALL(*observer,
              EmitStateBeginEvent(StrEq(kPendingFreezeTracingEventName),
                                  StrEq(kTitle)));
  lifecycle_unit.SetState(LifecycleUnitState::PENDING_FREEZE, kReason);
  Mock::VerifyAndClearExpectations(observer);

  EXPECT_CALL(*observer,
              EmitStateEndEvent(StrEq(kPendingFreezeTracingEventName)));
  lifecycle_unit.SetState(LifecycleUnitState::FROZEN, kReason);
  Mock::VerifyAndClearExpectations(observer);

  // No pending state.
  lifecycle_unit.SetState(LifecycleUnitState::ACTIVE, kReason);

  // No pending state.
  lifecycle_unit.SetState(LifecycleUnitState::FROZEN, kReason);

  // Pending unfreeze.
  EXPECT_CALL(*observer,
              EmitStateBeginEvent(StrEq(kPendingUnfreezeTracingEventName),
                                  StrEq(kTitle)));
  lifecycle_unit.SetState(LifecycleUnitState::PENDING_UNFREEZE, kReason);
  Mock::VerifyAndClearExpectations(observer);

  EXPECT_CALL(*observer,
              EmitStateEndEvent(StrEq(kPendingUnfreezeTracingEventName)));
  lifecycle_unit.SetState(LifecycleUnitState::ACTIVE, kReason);
  Mock::VerifyAndClearExpectations(observer);

  lifecycle_unit.SetState(LifecycleUnitState::DISCARDED, kReason);
  Mock::VerifyAndClearExpectations(observer);

  // Pending freeze twice.
  EXPECT_CALL(*observer,
              EmitStateBeginEvent(StrEq(kPendingFreezeTracingEventName),
                                  StrEq(kTitle)));
  lifecycle_unit.SetState(LifecycleUnitState::PENDING_FREEZE, kReason);
  Mock::VerifyAndClearExpectations(observer);

  lifecycle_unit.SetState(LifecycleUnitState::PENDING_FREEZE, kReason);

  // Destruction in a pending state.
  EXPECT_CALL(*observer,
              EmitStateEndEvent(StrEq(kPendingFreezeTracingEventName)));
}

}  // namespace resource_coordinator
