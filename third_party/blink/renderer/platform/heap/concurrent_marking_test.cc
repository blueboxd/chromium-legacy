// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class ConcurrentMarkingTest : public TestSupportingGC {};

namespace concurrent_marking_test {

template <typename T>
class CollectionWrapper : public GarbageCollected<CollectionWrapper<T>> {
 public:
  CollectionWrapper() : collection_(MakeGarbageCollected<T>()) {}

  void Trace(Visitor* visitor) { visitor->Trace(collection_); }

  T* GetCollection() { return collection_.Get(); }

 private:
  Member<T> collection_;
};

// =============================================================================
// Tests that expose data races when modifying collections =====================
// =============================================================================

template <typename C>
void AddToCollection() {
  constexpr int kIterations = 100;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < kIterations; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < kIterations; ++j) {
      int num = kIterations * i + j;
      collection->insert(MakeGarbageCollected<IntegerObject>(num));
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

template <typename C>
void RemoveFromCollection() {
  constexpr int kIterations = 100;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    collection->insert(MakeGarbageCollected<IntegerObject>(i));
  }
  driver.Start();
  for (int i = 0; i < kIterations; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < kIterations; ++j) {
      collection->erase(collection->begin());
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

template <typename C>
void SwapCollections() {
  constexpr int kIterations = 10;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    C* new_collection = MakeGarbageCollected<C>();
    for (int j = 0; j < kIterations * i; ++j) {
      new_collection->insert(MakeGarbageCollected<IntegerObject>(j));
    }
    driver.SingleConcurrentStep();
    collection->swap(*new_collection);
  }
  driver.FinishSteps();
  driver.FinishGC();
}

// HeapHashMap

template <typename T>
class HeapHashMapAdapter : public HeapHashMap<T, T> {
 public:
  template <typename U>
  ALWAYS_INLINE void insert(U* u) {
    HeapHashMap<T, T>::insert(u, u);
  }
};

TEST_F(ConcurrentMarkingTest, AddToHashMap) {
  AddToCollection<HeapHashMapAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, RemoveFromHashMap) {
  RemoveFromCollection<HeapHashMapAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, SwapHashMaps) {
  SwapCollections<HeapHashMapAdapter<Member<IntegerObject>>>();
}

// HeapHashSet

TEST_F(ConcurrentMarkingTest, AddToHashSet) {
  AddToCollection<HeapHashSet<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, RemoveFromHashSet) {
  RemoveFromCollection<HeapHashSet<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, SwapHashSets) {
  SwapCollections<HeapHashSet<Member<IntegerObject>>>();
}

// HeapLinkedHashSet
template <typename T>
class HeapLinkedHashSetAdapter : public HeapLinkedHashSet<T> {
 public:
  ALWAYS_INLINE void swap(HeapLinkedHashSetAdapter<T>& other) {
    HeapLinkedHashSet<T>::Swap(other);
  }
};

TEST_F(ConcurrentMarkingTest, AddToLinkedHashSet) {
  AddToCollection<HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, RemoveFromLinkedHashSet) {
  RemoveFromCollection<HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, SwapLinkedHashSets) {
  SwapCollections<HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}

// HeapListHashSet

template <typename T>
class HeapListHashSetAdapter : public HeapListHashSet<T> {
 public:
  ALWAYS_INLINE void swap(HeapListHashSetAdapter<T>& other) {
    HeapListHashSet<T>::Swap(other);
  }
};

TEST_F(ConcurrentMarkingTest, AddToListHashSet) {
  AddToCollection<HeapListHashSetAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, RemoveFromListHashSet) {
  RemoveFromCollection<HeapListHashSetAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, SwapListHashSets) {
  SwapCollections<HeapListHashSetAdapter<Member<IntegerObject>>>();
}

// HeapHashCountedSet

TEST_F(ConcurrentMarkingTest, AddToHashCountedSet) {
  AddToCollection<HeapHashCountedSet<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, RemoveFromHashCountedSet) {
  RemoveFromCollection<HeapHashCountedSet<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, SwapHashCountedSets) {
  SwapCollections<HeapHashCountedSet<Member<IntegerObject>>>();
}

// HeapVector

template <typename T>
class HeapVectorAdapter : public HeapVector<T> {
 public:
  template <typename U>
  ALWAYS_INLINE void insert(U* u) {
    HeapVector<T>::push_back(u);
  }
  ALWAYS_INLINE void erase(typename HeapVector<T>::iterator) {
    HeapVector<T>::pop_back();
  }
};

TEST_F(ConcurrentMarkingTest, AddToVector) {
  AddToCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, RemoveFromVector) {
  RemoveFromCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, SwapVectors) {
  SwapCollections<HeapVectorAdapter<Member<IntegerObject>>>();
}

// HeapDeque

template <typename T>
class HeapDequeAdapter : public HeapDeque<T> {
 public:
  template <typename U>
  ALWAYS_INLINE void insert(U* u) {
    HeapDeque<T>::push_back(u);
  }
  ALWAYS_INLINE void erase(typename HeapDeque<T>::iterator) {
    HeapDeque<T>::pop_back();
  }
  ALWAYS_INLINE void swap(HeapDequeAdapter<T>& other) {
    HeapDeque<T>::Swap(other);
  }
};

TEST_F(ConcurrentMarkingTest, AddToDeque) {
  AddToCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, RemoveFromDeque) {
  RemoveFromCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}

TEST_F(ConcurrentMarkingTest, SwapDeques) {
  SwapCollections<HeapDequeAdapter<Member<IntegerObject>>>();
}

}  // namespace concurrent_marking_test
}  // namespace blink
