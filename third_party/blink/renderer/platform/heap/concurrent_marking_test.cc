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
// Tests that expose data races when modifying collections ================
// =============================================================================

TEST_F(ConcurrentMarkingTest, AddToHashMap) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using Map = HeapHashMap<Member<IntegerObject>, Member<IntegerObject>>;
  Persistent<CollectionWrapper<Map>> persistent =
      MakeGarbageCollected<CollectionWrapper<Map>>();
  Map* map = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < 100; ++j) {
      int num = 100 * i + j;
      map->insert(MakeGarbageCollected<IntegerObject>(num),
                  MakeGarbageCollected<IntegerObject>(num));
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, RemoveFromHashMap) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using Map = HeapHashMap<Member<IntegerObject>, Member<IntegerObject>>;
  Persistent<CollectionWrapper<Map>> persistent =
      MakeGarbageCollected<CollectionWrapper<Map>>();
  Map* map = persistent->GetCollection();
  for (int i = 0; i < 10000; ++i) {
    map->insert(MakeGarbageCollected<IntegerObject>(i),
                MakeGarbageCollected<IntegerObject>(i));
  }
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < 100; ++j) {
      map->erase(map->begin());
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, SwapHashMaps) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using Map = HeapHashMap<Member<IntegerObject>, Member<IntegerObject>>;
  Persistent<CollectionWrapper<Map>> persistent =
      MakeGarbageCollected<CollectionWrapper<Map>>();
  Map* map = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    Map new_map;
    for (int j = 0; j < 10 * i; ++j) {
      new_map.insert(MakeGarbageCollected<IntegerObject>(j),
                     MakeGarbageCollected<IntegerObject>(j));
    }
    driver.SingleConcurrentStep();
    map->swap(new_map);
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, AddToHashSet) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using Set = HeapHashSet<Member<IntegerObject>>;
  Persistent<CollectionWrapper<Set>> persistent =
      MakeGarbageCollected<CollectionWrapper<Set>>();
  Set* set = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < 100; ++j) {
      int num = 100 * i + j;
      set->insert(MakeGarbageCollected<IntegerObject>(num));
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, RemoveFromHashSet) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using Set = HeapHashSet<Member<IntegerObject>>;
  Persistent<CollectionWrapper<Set>> persistent =
      MakeGarbageCollected<CollectionWrapper<Set>>();
  Set* set = persistent->GetCollection();
  for (int i = 0; i < 10000; ++i) {
    set->insert(MakeGarbageCollected<IntegerObject>(i));
  }
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < 100; ++j) {
      set->erase(set->begin());
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, SwapHashSets) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using Set = HeapHashSet<Member<IntegerObject>>;
  Persistent<CollectionWrapper<Set>> persistent =
      MakeGarbageCollected<CollectionWrapper<Set>>();
  Set* set = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    Set new_set;
    for (int j = 0; j < 10 * i; ++j) {
      new_set.insert(MakeGarbageCollected<IntegerObject>(j));
    }
    driver.SingleConcurrentStep();
    set->swap(new_set);
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, AddToVector) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using V = HeapVector<Member<IntegerObject>>;
  Persistent<CollectionWrapper<V>> persistent =
      MakeGarbageCollected<CollectionWrapper<V>>();
  V* vector = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < 100; ++j) {
      int num = 100 * i + j;
      vector->push_back(MakeGarbageCollected<IntegerObject>(num));
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, RemoveFromVector) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using V = HeapVector<Member<IntegerObject>>;
  Persistent<CollectionWrapper<V>> persistent =
      MakeGarbageCollected<CollectionWrapper<V>>();
  V* vector = persistent->GetCollection();
  for (int i = 0; i < 10000; ++i) {
    vector->push_back(MakeGarbageCollected<IntegerObject>(i));
  }
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < 100; ++j) {
      vector->pop_back();
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(ConcurrentMarkingTest, SwapVectors) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  using V = HeapVector<Member<IntegerObject>>;
  Persistent<CollectionWrapper<V>> persistent =
      MakeGarbageCollected<CollectionWrapper<V>>();
  V* vector = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < 100; ++i) {
    V new_vector;
    for (int j = 0; j < 10 * i; ++j) {
      new_vector.push_back(MakeGarbageCollected<IntegerObject>(j));
    }
    driver.SingleConcurrentStep();
    vector->swap(new_vector);
  }
  driver.FinishSteps();
  driver.FinishGC();
}

}  // namespace concurrent_marking_test
}  // namespace blink
