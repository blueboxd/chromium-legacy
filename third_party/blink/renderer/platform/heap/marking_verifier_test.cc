// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/marking_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class MarkingVerifierDeathTest : public TestSupportingGC {};

namespace {

class ResurrectingPreFinalizer
    : public GarbageCollected<ResurrectingPreFinalizer> {
  USING_PRE_FINALIZER(ResurrectingPreFinalizer, Dispose);

 public:
  enum TestType {
    kMember,
    kWeakMember,
    kHeapVectorMember,
    kHeapHashSetMember,
    kHeapHashSetWeakMember
  };

  class GlobalStorage : public GarbageCollected<GlobalStorage> {
   public:
    GlobalStorage() {
      // Reserve storage upfront to avoid allocations during pre-finalizer
      // insertion.
      vector_member.ReserveCapacity(32);
      hash_set_member.ReserveCapacityForSize(32);
      hash_set_weak_member.ReserveCapacityForSize(32);
    }

    void Trace(Visitor* visitor) {
      visitor->Trace(strong);
      visitor->Trace(weak);
      visitor->Trace(vector_member);
      visitor->Trace(hash_set_member);
      visitor->Trace(hash_set_weak_member);
    }

    Member<LinkedObject> strong;
    WeakMember<LinkedObject> weak;
    HeapVector<Member<LinkedObject>> vector_member;
    HeapHashSet<Member<LinkedObject>> hash_set_member;
    HeapHashSet<WeakMember<LinkedObject>> hash_set_weak_member;
  };

  ResurrectingPreFinalizer(TestType test_type,
                           GlobalStorage* storage,
                           LinkedObject* object_that_dies)
      : test_type_(test_type),
        storage_(storage),
        object_that_dies_(object_that_dies) {}

  void Trace(Visitor* visitor) {
    visitor->Trace(storage_);
    visitor->Trace(object_that_dies_);
  }

 private:
  void Dispose() {
    switch (test_type_) {
      case TestType::kMember:
        storage_->strong = object_that_dies_;
        break;
      case TestType::kWeakMember:
        storage_->weak = object_that_dies_;
        break;
      case TestType::kHeapVectorMember:
        storage_->vector_member.push_back(object_that_dies_);
        break;
      case TestType::kHeapHashSetMember:
        storage_->hash_set_member.insert(object_that_dies_);
        break;
      case TestType::kHeapHashSetWeakMember:
        storage_->hash_set_weak_member.insert(object_that_dies_);
        break;
    }
  }

  TestType test_type_;
  Member<GlobalStorage> storage_;
  Member<LinkedObject> object_that_dies_;
};

}  // namespace

TEST_F(MarkingVerifierDeathTest, DiesOnResurrectedMember) {
  if (!ThreadState::Current()->VerifyMarkingEnabled())
    return;

  Persistent<ResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<ResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<ResurrectingPreFinalizer>(
      ResurrectingPreFinalizer::kMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  ASSERT_DEATH_IF_SUPPORTED(PreciselyCollectGarbage(),
                            "MarkingVerifier: Encountered unmarked object.");
}

TEST_F(MarkingVerifierDeathTest, DiesOnResurrectedWeakMember) {
  if (!ThreadState::Current()->VerifyMarkingEnabled())
    return;

  Persistent<ResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<ResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<ResurrectingPreFinalizer>(
      ResurrectingPreFinalizer::kWeakMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  ASSERT_DEATH_IF_SUPPORTED(PreciselyCollectGarbage(),
                            "MarkingVerifier: Encountered unmarked object.");
}

TEST_F(MarkingVerifierDeathTest, DiesOnResurrectedHeapVectorMember) {
  if (!ThreadState::Current()->VerifyMarkingEnabled())
    return;

  Persistent<ResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<ResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<ResurrectingPreFinalizer>(
      ResurrectingPreFinalizer::kHeapVectorMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  ASSERT_DEATH_IF_SUPPORTED(PreciselyCollectGarbage(),
                            "MarkingVerifier: Encountered unmarked object.");
}

TEST_F(MarkingVerifierDeathTest, DiesOnResurrectedHeapHashSetMember) {
  if (!ThreadState::Current()->VerifyMarkingEnabled())
    return;

  Persistent<ResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<ResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<ResurrectingPreFinalizer>(
      ResurrectingPreFinalizer::kHeapHashSetMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  ASSERT_DEATH_IF_SUPPORTED(PreciselyCollectGarbage(),
                            "MarkingVerifier: Encountered unmarked object.");
}

TEST_F(MarkingVerifierDeathTest, DiesOnResurrectedHeapHashSetWeakMember) {
  if (!ThreadState::Current()->VerifyMarkingEnabled())
    return;

  Persistent<ResurrectingPreFinalizer::GlobalStorage> storage(
      MakeGarbageCollected<ResurrectingPreFinalizer::GlobalStorage>());
  MakeGarbageCollected<ResurrectingPreFinalizer>(
      ResurrectingPreFinalizer::kHeapHashSetWeakMember, storage.Get(),
      MakeGarbageCollected<LinkedObject>());
  ASSERT_DEATH_IF_SUPPORTED(PreciselyCollectGarbage(),
                            "MarkingVerifier: Encountered unmarked object.");
}

}  // namespace blink
