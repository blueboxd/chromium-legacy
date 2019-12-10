// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

// ChildLocalSurfaceIdAllocator has 1 accessor which does not alter state:
// - GetCurrentLocalSurfaceIdAllocation()
//
// For every operation which changes state we can test:
// - the operation completed as expected,
// - the accessors did not change, and/or
// - the accessors changed in the way we expected.

namespace viz {
namespace {

::testing::AssertionResult ParentSequenceNumberIsNotSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult EmbedTokenIsValid(
    const LocalSurfaceId& local_surface_id);

}  // namespace

class ChildLocalSurfaceIdAllocatorTest : public testing::Test {
 public:
  ChildLocalSurfaceIdAllocatorTest() = default;

  ~ChildLocalSurfaceIdAllocatorTest() override {}

  ChildLocalSurfaceIdAllocator& allocator() { return *allocator_.get(); }

  ParentLocalSurfaceIdAllocator& parent_allocator1() {
    return *parent_allocator1_.get();
  }

  ParentLocalSurfaceIdAllocator& parent_allocator2() {
    return *parent_allocator2_.get();
  }

  base::TimeTicks Now() { return now_src_->NowTicks(); }

  void AdvanceTime(base::TimeDelta delta) { now_src_->Advance(delta); }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    now_src_ = std::make_unique<base::SimpleTestTickClock>();
    // Advance time by one millisecond to ensure all time stamps are non-null.
    AdvanceTime(base::TimeDelta::FromMilliseconds(1u));
    allocator_ = std::make_unique<ChildLocalSurfaceIdAllocator>(now_src_.get());
    parent_allocator1_ =
        std::make_unique<ParentLocalSurfaceIdAllocator>(now_src_.get());
    parent_allocator2_ =
        std::make_unique<ParentLocalSurfaceIdAllocator>(now_src_.get());
  }

  void TearDown() override {
    parent_allocator2_.reset();
    parent_allocator1_.reset();
    allocator_.reset();
    now_src_.reset();
  }

 private:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  std::unique_ptr<ChildLocalSurfaceIdAllocator> allocator_;
  std::unique_ptr<ParentLocalSurfaceIdAllocator> parent_allocator1_;
  std::unique_ptr<ParentLocalSurfaceIdAllocator> parent_allocator2_;

  DISALLOW_COPY_AND_ASSIGN(ChildLocalSurfaceIdAllocatorTest);
};

// The default constructor should initialize its last-known LocalSurfaceId (and
// all of its components) to an invalid state.
TEST_F(ChildLocalSurfaceIdAllocatorTest,
       DefaultConstructorShouldNotSetLocalSurfaceIdComponents) {
  const LocalSurfaceId& default_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_FALSE(default_local_surface_id.is_valid());
  EXPECT_TRUE(ParentSequenceNumberIsNotSet(default_local_surface_id));
  EXPECT_TRUE(ChildSequenceNumberIsSet(default_local_surface_id));
  EXPECT_FALSE(EmbedTokenIsValid(default_local_surface_id));
}

// UpdateFromParent() on a child allocator should accept the parent's sequence
// number and embed_token. But it should continue to use its own child sequence
// number.
TEST_F(ChildLocalSurfaceIdAllocatorTest,
       UpdateFromParentOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  LocalSurfaceId preupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();

  parent_allocator1().GenerateId();
  LocalSurfaceId parent_allocated_local_surface_id =
      parent_allocator1()
          .GetCurrentLocalSurfaceIdAllocation()
          .local_surface_id();
  EXPECT_NE(preupdate_local_surface_id.parent_sequence_number(),
            parent_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.embed_token(),
            parent_allocated_local_surface_id.embed_token());

  bool changed = allocator().UpdateFromParent(
      parent_allocator1().GetCurrentLocalSurfaceIdAllocation());
  EXPECT_TRUE(changed);

  const LocalSurfaceId& postupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_EQ(postupdate_local_surface_id.parent_sequence_number(),
            parent_allocated_local_surface_id.parent_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.embed_token(),
            parent_allocated_local_surface_id.embed_token());
}

// UpdateFromParent() on a child allocator should accept the parent's
// LocalSurfaceId if only the embed_token changed.
TEST_F(ChildLocalSurfaceIdAllocatorTest, UpdateFromParentEmbedTokenChanged) {
  parent_allocator1().GenerateId();
  EXPECT_TRUE(parent_allocator1()
                  .GetCurrentLocalSurfaceIdAllocation()
                  .local_surface_id()
                  .is_valid());
  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator1().GetCurrentLocalSurfaceIdAllocation()));
  parent_allocator2().GenerateId();
  EXPECT_LE(parent_allocator2()
                .GetCurrentLocalSurfaceIdAllocation()
                .local_surface_id()
                .parent_sequence_number(),
            parent_allocator1()
                .GetCurrentLocalSurfaceIdAllocation()
                .local_surface_id()
                .parent_sequence_number());
  EXPECT_NE(parent_allocator2()
                .GetCurrentLocalSurfaceIdAllocation()
                .local_surface_id()
                .embed_token(),
            parent_allocator1()
                .GetCurrentLocalSurfaceIdAllocation()
                .local_surface_id()
                .embed_token());

  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator2().GetCurrentLocalSurfaceIdAllocation()));
}

// GenerateId() on a child allocator should monotonically increment the child
// sequence number.
TEST_F(ChildLocalSurfaceIdAllocatorTest,
       GenerateIdOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  parent_allocator1().GenerateId();
  allocator().UpdateFromParent(
      parent_allocator1().GetCurrentLocalSurfaceIdAllocation());
  LocalSurfaceId pregenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();

  allocator().GenerateId();
  const LocalSurfaceId& returned_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();

  const LocalSurfaceId& postgenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_EQ(pregenerateid_local_surface_id.parent_sequence_number(),
            postgenerateid_local_surface_id.parent_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.child_sequence_number() + 1,
            postgenerateid_local_surface_id.child_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.embed_token(),
            postgenerateid_local_surface_id.embed_token());
  EXPECT_EQ(
      returned_local_surface_id,
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id());
}

// This test verifies that if the parent-allocated LocalSurfaceId has the most
// recent child sequence number at the time UpdateFromParent is called, then
// its allocation time is used as the latest allocation time in
// ChildLocalSurfaceIdAllocator. In the event that the parent-allocated
// LocalSurfaceId does not correspond to the latest child sequence number
// then UpdateFromParent represents a new allocation and thus the allocation
// time is updated.
TEST_F(ChildLocalSurfaceIdAllocatorTest,
       CorrectTimeStampUsedInUpdateFromParent) {
  // Parent allocates an ID.
  parent_allocator1().GenerateId();
  LocalSurfaceId parent_allocated_id = parent_allocator1()
                                           .GetCurrentLocalSurfaceIdAllocation()
                                           .local_surface_id();
  base::TimeTicks parent_allocation_time =
      parent_allocator1()
          .GetCurrentLocalSurfaceIdAllocation()
          .allocation_time();

  // Advance time by one millisecond.
  AdvanceTime(base::TimeDelta::FromMilliseconds(1u));

  // Update the child ID from the parent.
  {
    bool changed = allocator().UpdateFromParent(
        parent_allocator1().GetCurrentLocalSurfaceIdAllocation());
    EXPECT_TRUE(changed);
    EXPECT_EQ(
        parent_allocated_id,
        allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id());
    EXPECT_EQ(
        parent_allocation_time,
        allocator().GetCurrentLocalSurfaceIdAllocation().allocation_time());
  }

  // The parent allocator allocates another ID.
  parent_allocator1().GenerateId();
  LocalSurfaceId parent_allocated_id2 =
      parent_allocator1()
          .GetCurrentLocalSurfaceIdAllocation()
          .local_surface_id();

  // The child allocator allocates another ID.
  allocator().GenerateId();

  // Advance time by one millisecond.
  AdvanceTime(base::TimeDelta::FromMilliseconds(1u));

  // Update the child allocator's ID from the parent. The merged LocalSurfaceId
  // does not correspond to neither of parent's LocalSurfaceId or child's
  // previous LocalSurfaceId, so it should be treated as a new allocation and
  // have a new timestamp.
  {
    bool changed = allocator().UpdateFromParent(
        LocalSurfaceIdAllocation(parent_allocated_id2, parent_allocation_time));
    EXPECT_TRUE(changed);
    EXPECT_NE(
        parent_allocated_id2,
        allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id());
    EXPECT_EQ(parent_allocated_id2.parent_sequence_number(),
              allocator()
                  .GetCurrentLocalSurfaceIdAllocation()
                  .local_surface_id()
                  .parent_sequence_number());
    EXPECT_EQ(
        Now(),
        allocator().GetCurrentLocalSurfaceIdAllocation().allocation_time());
  }
}

TEST_F(ChildLocalSurfaceIdAllocatorTest, EmbedTokenChangeResetsChildId) {
  parent_allocator1().GenerateId();
  EXPECT_TRUE(parent_allocator1()
                  .GetCurrentLocalSurfaceIdAllocation()
                  .local_surface_id()
                  .is_valid());
  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator1().GetCurrentLocalSurfaceIdAllocation()));
  allocator().GenerateId();

  EXPECT_NE(1u, allocator()
                    .GetCurrentLocalSurfaceIdAllocation()
                    .local_surface_id()
                    .child_sequence_number());

  parent_allocator2().GenerateId();

  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator2().GetCurrentLocalSurfaceIdAllocation()));

  EXPECT_EQ(1u, allocator()
                    .GetCurrentLocalSurfaceIdAllocation()
                    .local_surface_id()
                    .child_sequence_number());
}

namespace {

::testing::AssertionResult ParentSequenceNumberIsNotSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.parent_sequence_number() == kInvalidParentSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "parent_sequence_number() is set.";
}

::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.child_sequence_number() != kInvalidChildSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "child_sequence_number() is not set.";
}

::testing::AssertionResult EmbedTokenIsValid(
    const LocalSurfaceId& local_surface_id) {
  if (!local_surface_id.embed_token().is_empty())
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "embed_token() is not valid.";
}

}  // namespace
}  // namespace viz
