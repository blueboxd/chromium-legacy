// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

class SimpleGCedBase : public GarbageCollected<SimpleGCedBase> {
 public:
  static size_t destructed_objects;

  virtual ~SimpleGCedBase() { ++destructed_objects; }

  void Trace(Visitor* v) { v->Trace(next); }

  Member<SimpleGCedBase> next;
};

size_t SimpleGCedBase::destructed_objects;

template <size_t Size>
class SimpleGCed final : public SimpleGCedBase {
  char array[Size];
};

using Small = SimpleGCed<64>;
using Large = SimpleGCed<1024 * 1024>;

template <typename Type>
struct OtherType;
template <>
struct OtherType<Small> {
  using Type = Large;
};
template <>
struct OtherType<Large> {
  using Type = Small;
};

class MinorGCTest : public TestSupportingGC {
 public:
  MinorGCTest() {
    ClearOutOldGarbage();
    SimpleGCedBase::destructed_objects = 0;
  }

  static size_t DestructedObjects() {
    return SimpleGCedBase::destructed_objects;
  }

  static void CollectMinor() { Collect(BlinkGC::CollectionType::kMinor); }
  static void CollectMajor() { Collect(BlinkGC::CollectionType::kMajor); }

 private:
  static void Collect(BlinkGC::CollectionType type) {
    ThreadState::Current()->CollectGarbage(
        type, BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
        BlinkGC::kEagerSweeping, BlinkGC::GCReason::kForcedGCForTesting);
  }
};

template <typename SmallOrLarge>
class MinorGCTestForType : public MinorGCTest {
 public:
  using Type = SmallOrLarge;
};

}  // namespace

using ObjectTypes = ::testing::Types<Small, Large>;
TYPED_TEST_SUITE(MinorGCTestForType, ObjectTypes);

TYPED_TEST(MinorGCTestForType, MinorCollection) {
  using Type = typename TestFixture::Type;

  MakeGarbageCollected<Type>();
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  MinorGCTest::CollectMinor();
  EXPECT_EQ(1u, TestFixture::DestructedObjects());

  Type* prev = nullptr;
  for (size_t i = 0; i < 64; ++i) {
    auto* ptr = MakeGarbageCollected<Type>();
    ptr->next = prev;
    prev = ptr;
  }

  MinorGCTest::CollectMinor();
  EXPECT_EQ(65u, TestFixture::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, StickyBits) {
  using Type = typename TestFixture::Type;

  Persistent<Type> p1 = MakeGarbageCollected<Type>();
  TestFixture::CollectMinor();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(p1.Get())->IsMarked());
  TestFixture::CollectMajor();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(p1.Get())->IsMarked());
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, OldObjectIsNotVisited) {
  using Type = typename TestFixture::Type;

  Persistent<Type> p = MakeGarbageCollected<Type>();
  TestFixture::CollectMinor();
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  EXPECT_TRUE(HeapObjectHeader::FromPayload(p.Get())->IsMarked());

  // Check that the old deleted object won't be visited during minor GC.
  Type* raw = p.Release();
  TestFixture::CollectMinor();
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  EXPECT_TRUE(HeapObjectHeader::FromPayload(raw)->IsMarked());
  EXPECT_FALSE(HeapObjectHeader::FromPayload(raw)->IsFree());

  // Check that the old deleted object will be revisited in major GC.
  TestFixture::CollectMajor();
  EXPECT_EQ(1u, TestFixture::DestructedObjects());
}

template <typename Type1, typename Type2>
void InterGenerationalPointerTest() {
  Persistent<Type1> old = MakeGarbageCollected<Type1>();
  MinorGCTest::CollectMinor();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(old.Get())->IsMarked());

  // Allocate young objects.
  Type2* young = nullptr;
  for (size_t i = 0; i < 64; ++i) {
    auto* ptr = MakeGarbageCollected<Type2>();
    ptr->next = young;
    young = ptr;
    EXPECT_FALSE(HeapObjectHeader::FromPayload(young)->IsMarked());
  }

  // Issue generational barrier.
  old->next = young;

  // Check that the remembered set is visited.
  MinorGCTest::CollectMinor();
  EXPECT_EQ(0u, MinorGCTest::DestructedObjects());
  for (size_t i = 0; i < 64; ++i) {
    EXPECT_TRUE(HeapObjectHeader::FromPayload(young)->IsMarked());
    EXPECT_FALSE(HeapObjectHeader::FromPayload(young)->IsFree());
    young = static_cast<Type2*>(young->next.Get());
  }

  old.Release();
  MinorGCTest::CollectMajor();
  EXPECT_EQ(65u, MinorGCTest::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerForSamePageTypes) {
  using Type = typename TestFixture::Type;
  InterGenerationalPointerTest<Type, Type>();
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerForDifferentPageTypes) {
  using Type = typename TestFixture::Type;
  InterGenerationalPointerTest<Type, typename OtherType<Type>::Type>();
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerInCollection) {
  using Type = typename TestFixture::Type;

  static constexpr size_t kCollectionSize = 128;
  Persistent<HeapVector<Member<Type>>> old =
      MakeGarbageCollected<HeapVector<Member<Type>>>();
  old->resize(kCollectionSize);
  void* raw_backing = old->data();
  EXPECT_FALSE(HeapObjectHeader::FromPayload(raw_backing)->IsMarked());
  MinorGCTest::CollectMinor();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(raw_backing)->IsMarked());

  // Issue barrier for every second member.
  size_t i = 0;
  for (auto& member : *old) {
    if (i % 2) {
      member = MakeGarbageCollected<Type>();
    } else {
      MakeGarbageCollected<Type>();
    }
    ++i;
  }

  // Check that the remembered set is visited.
  MinorGCTest::CollectMinor();
  EXPECT_EQ(kCollectionSize / 2, MinorGCTest::DestructedObjects());
  for (auto& member : *old) {
    if (member) {
      EXPECT_TRUE(HeapObjectHeader::FromPayload(member.Get())->IsMarked());
      EXPECT_FALSE(HeapObjectHeader::FromPayload(member.Get())->IsFree());
    }
  }

  old.Release();
  MinorGCTest::CollectMajor();
  EXPECT_EQ(kCollectionSize, MinorGCTest::DestructedObjects());
}

}  // namespace blink
