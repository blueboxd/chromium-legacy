// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <iostream>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

namespace {

class WeaknessMarkingTest : public TestSupportingGC {};

}  // namespace

enum class ObjectLiveness { Alive = 0, Dead };

template <typename Map,
          template <typename T>
          class KeyHolder,
          template <typename T>
          class ValueHolder>
void TestMapImpl(ObjectLiveness expected_key_liveness,
                 ObjectLiveness expected_value_liveness) {
  Persistent<Map> map = MakeGarbageCollected<Map>();
  KeyHolder<IntegerObject> int_key = MakeGarbageCollected<IntegerObject>(1);
  ValueHolder<IntegerObject> int_value = MakeGarbageCollected<IntegerObject>(2);
  map->insert(int_key.Get(), int_value.Get());
  TestSupportingGC::PreciselyCollectGarbage();
  if (expected_key_liveness == ObjectLiveness::Alive) {
    EXPECT_TRUE(int_key.Get());
  } else {
    EXPECT_FALSE(int_key.Get());
  }
  if (expected_value_liveness == ObjectLiveness::Alive) {
    EXPECT_TRUE(int_value.Get());
  } else {
    EXPECT_FALSE(int_value.Get());
  }
  EXPECT_EQ(((expected_key_liveness == ObjectLiveness::Alive) &&
             (expected_value_liveness == ObjectLiveness::Alive))
                ? 1u
                : 0u,
            map->size());
}

TEST_F(WeaknessMarkingTest, WeakToWeakMap) {
  typedef HeapHashMap<WeakMember<IntegerObject>, WeakMember<IntegerObject>> Map;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Dead,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Dead);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Dead,
                                                   ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, WeakToStrongMap) {
  typedef HeapHashMap<WeakMember<IntegerObject>, Member<IntegerObject>> Map;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Dead,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Dead,
                                                   ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, StrongToWeakMap) {
  typedef HeapHashMap<Member<IntegerObject>, WeakMember<IntegerObject>> Map;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Dead);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Dead,
                                                   ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, StrongToStrongMap) {
  typedef HeapHashMap<Member<IntegerObject>, Member<IntegerObject>> Map;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Alive,
                                                   ObjectLiveness::Alive);
}

template <typename Set, template <typename T> class Type>
void TestSetImpl(ObjectLiveness object_liveness) {
  Persistent<Set> set = MakeGarbageCollected<Set>();
  Type<IntegerObject> object = MakeGarbageCollected<IntegerObject>(1);
  set->insert(object.Get());
  TestSupportingGC::PreciselyCollectGarbage();
  if (object_liveness == ObjectLiveness::Alive) {
    EXPECT_TRUE(object.Get());
  } else {
    EXPECT_FALSE(object.Get());
  }
  EXPECT_EQ((object_liveness == ObjectLiveness::Alive) ? 1u : 0u, set->size());
}

TEST_F(WeaknessMarkingTest, WeakSet) {
  typedef HeapHashSet<WeakMember<IntegerObject>> Set;
  TestSetImpl<Set, Persistent>(ObjectLiveness::Alive);
  TestSetImpl<Set, WeakPersistent>(ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, StrongSet) {
  typedef HeapHashSet<Member<IntegerObject>> Set;
  TestSetImpl<Set, Persistent>(ObjectLiveness::Alive);
  TestSetImpl<Set, WeakPersistent>(ObjectLiveness::Alive);
}

}  // namespace blink
