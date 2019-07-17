// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ThreadStateSchedulingTest : public TestSupportingGC {
 public:
  void SetUp() override {
    state_ = ThreadState::Current();
    ClearOutOldGarbage();
    initial_gc_age_ = state_->GcAge();
  }

  void TearDown() override {
    PreciselyCollectGarbage();
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    EXPECT_FALSE(state_->IsMarkingInProgress());
    EXPECT_FALSE(state_->IsSweepingInProgress());
  }

  void StartIncrementalMarking() {
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    state_->ScheduleIncrementalGC(BlinkGC::GCReason::kForcedGCForTesting);
    state_->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
    EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
              state_->GetGCState());
    EXPECT_TRUE(state_->IsMarkingInProgress());
  }

  void StartLazySweepingForPreciseGC() {
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    state_->SchedulePreciseGC();
    EXPECT_EQ(ThreadState::kPreciseGCScheduled, state_->GetGCState());
    RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
    EXPECT_TRUE(state_->IsSweepingInProgress());
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
  }

  void RunScheduledGC(BlinkGC::StackState stack_state) {
    state_->RunScheduledGC(stack_state);
  }

  // Counter that is incremented when sweep finishes.
  int GCCount() { return state_->GcAge() - initial_gc_age_; }

  ThreadState* state() { return state_; }

 private:
  ThreadState* state_;
  int initial_gc_age_;
};

TEST_F(ThreadStateSchedulingTest, ScheduleIncrementalV8FollowupGCAgain) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->ScheduleIncrementalGC(
      BlinkGC::GCReason::kIncrementalV8FollowupGC);
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());

  // Calling ScheduleIncrementalV8FollowupGC() while one is already scheduled
  // will do nothing.
  test->state()->ScheduleIncrementalGC(
      BlinkGC::GCReason::kIncrementalV8FollowupGC);

  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());
  EXPECT_EQ(0, test->GCCount());
}

TEST_F(ThreadStateSchedulingTest, SchedulePreciseGCWhileLazySweeping) {
  ThreadStateSchedulingTest* test = this;

  test->StartLazySweepingForPreciseGC();

  test->state()->SchedulePreciseGC();

  // Scheduling a precise GC should finish lazy sweeping.
  EXPECT_FALSE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kPreciseGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest,
       ScheduleIncrementalV8FollowupGCWhileLazySweeping) {
  ThreadStateSchedulingTest* test = this;

  test->StartLazySweepingForPreciseGC();

  test->state()->ScheduleIncrementalGC(
      BlinkGC::GCReason::kIncrementalV8FollowupGC);

  // Scheduling a IncrementalV8FollowupGC should finish lazy sweeping.
  EXPECT_FALSE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest, SchedulePreciseGCWhileIncrementalMarking) {
  ThreadStateSchedulingTest* test = this;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kBlinkHeapIncrementalMarking);

  test->StartIncrementalMarking();
  test->state()->SchedulePreciseGC();
  // Scheduling a precise GC should cancel incremental marking tasks.
  EXPECT_EQ(ThreadState::kPreciseGCScheduled, test->state()->GetGCState());

  EXPECT_EQ(0, test->GCCount());
  test->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
  EXPECT_TRUE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());

  // Running the precise GC should simply finish the incremental marking idle GC
  // (not run another GC).
  EXPECT_EQ(0, test->GCCount());
  test->state()->CompleteSweep();
  EXPECT_EQ(1, test->GCCount());
}

TEST_F(ThreadStateSchedulingTest,
       ScheduleIncrementalV8FollowupGCWhileIncrementalMarking) {
  ThreadStateSchedulingTest* test = this;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kBlinkHeapIncrementalMarking);

  test->StartIncrementalMarking();
  test->state()->ScheduleIncrementalGC(
      BlinkGC::GCReason::kIncrementalV8FollowupGC);
  // Scheduling another incremental GC should not cancel incremental marking
  // tasks.
  EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
            test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest,
       ScheduleIncrementalV8FollowupGCWhileGCForbidden) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->ScheduleIncrementalGC(
      BlinkGC::GCReason::kIncrementalV8FollowupGC);
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());

  ThreadState::GCForbiddenScope gc_forbidden_scope(test->state());
  test->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);

  // Starting an IncrementalV8FollowupGC while GC is forbidden should do
  // nothing.
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());
  EXPECT_EQ(0, GCCount());
}

TEST_F(ThreadStateSchedulingTest, RunIncrementalV8FollowupGC) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->ScheduleIncrementalGC(
      BlinkGC::GCReason::kIncrementalV8FollowupGC);
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());

  test->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);

  EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
            test->state()->GetGCState());
}

}  // namespace blink
