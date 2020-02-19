/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/platform/heap_observer_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class TestingObserver;

class TestingNotifier final : public GarbageCollected<TestingNotifier> {
 public:
  TestingNotifier() = default;

  HeapObserverSet<TestingObserver>& ObserverSet() { return observer_set_; }

  void Trace(Visitor* visitor) { visitor->Trace(observer_set_); }

 private:
  HeapObserverSet<TestingObserver> observer_set_;
};

class TestingObserver final : public GarbageCollected<TestingObserver> {
 public:
  TestingObserver() = default;
  void OnNotification() { count_++; }
  int Count() { return count_; }
  void Trace(Visitor* visitor) {}

 private:
  int count_ = 0;
};

void Notify(HeapObserverSet<TestingObserver>& observer_set) {
  observer_set.ForEachObserver(
      [](TestingObserver* observer) { observer->OnNotification(); });
}

TEST(HeapObserverSetTest, AddRemove) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();

  notifier->ObserverSet().AddObserver(observer);

  EXPECT_EQ(observer->Count(), 0);
  Notify(notifier->ObserverSet());
  EXPECT_EQ(observer->Count(), 1);

  notifier->ObserverSet().RemoveObserver(observer);

  Notify(notifier->ObserverSet());
  EXPECT_EQ(observer->Count(), 1);
}

TEST(HeapObserverSetTest, HasObserver) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();

  EXPECT_FALSE(notifier->ObserverSet().HasObserver(observer));

  notifier->ObserverSet().AddObserver(observer);
  EXPECT_TRUE(notifier->ObserverSet().HasObserver(observer.Get()));

  notifier->ObserverSet().RemoveObserver(observer);
  EXPECT_FALSE(notifier->ObserverSet().HasObserver(observer.Get()));
}

TEST(HeapObserverSetTest, GarbageCollect) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();
  notifier->ObserverSet().AddObserver(observer);

  ThreadState::Current()->CollectAllGarbageForTesting();
  Notify(notifier->ObserverSet());
  EXPECT_EQ(observer->Count(), 1);

  WeakPersistent<TestingObserver> weak_ref = observer.Get();
  observer = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(weak_ref.Get(), nullptr);
}

TEST(HeapObserverSetTest, IsIteratingOverObservers) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer =
      MakeGarbageCollected<TestingObserver>();
  notifier->ObserverSet().AddObserver(observer);

  EXPECT_FALSE(notifier->ObserverSet().IsIteratingOverObservers());
  notifier->ObserverSet().ForEachObserver([&](TestingObserver* observer) {
    EXPECT_TRUE(notifier->ObserverSet().IsIteratingOverObservers());
  });
}

TEST(HeapObserverSetTest, ForEachObserver) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer1 =
      MakeGarbageCollected<TestingObserver>();
  Persistent<TestingObserver> observer2 =
      MakeGarbageCollected<TestingObserver>();

  HeapHashSet<Member<TestingObserver>> seen_observers;

  notifier->ObserverSet().AddObserver(observer1);
  notifier->ObserverSet().AddObserver(observer2);
  notifier->ObserverSet().ForEachObserver(
      [&](TestingObserver* observer) { seen_observers.insert(observer); });

  ASSERT_EQ(2u, seen_observers.size());
  EXPECT_TRUE(seen_observers.Contains(observer1));
  EXPECT_TRUE(seen_observers.Contains(observer2));
}

TEST(HeapObserverSetTest, RemoveWhileIterating) {
  Persistent<TestingNotifier> notifier =
      MakeGarbageCollected<TestingNotifier>();
  Persistent<TestingObserver> observer1 =
      MakeGarbageCollected<TestingObserver>();
  Persistent<TestingObserver> observer2 =
      MakeGarbageCollected<TestingObserver>();

  notifier->ObserverSet().AddObserver(observer1);
  notifier->ObserverSet().AddObserver(observer2);
  notifier->ObserverSet().ForEachObserver([&](TestingObserver* observer) {
    EXPECT_TRUE(notifier->ObserverSet().HasObserver(observer));
    notifier->ObserverSet().RemoveObserver(observer);
    EXPECT_FALSE(notifier->ObserverSet().HasObserver(observer));
  });
  EXPECT_FALSE(notifier->ObserverSet().HasObserver(observer1));
  EXPECT_FALSE(notifier->ObserverSet().HasObserver(observer2));

  notifier->ObserverSet().AddObserver(observer1);
  notifier->ObserverSet().AddObserver(observer2);
  notifier->ObserverSet().ForEachObserver([&](TestingObserver* observer) {
    EXPECT_TRUE(notifier->ObserverSet().HasObserver(observer1));
    EXPECT_TRUE(notifier->ObserverSet().HasObserver(observer2));
    notifier->ObserverSet().RemoveObserver(observer1);
    EXPECT_FALSE(notifier->ObserverSet().HasObserver(observer1));
    notifier->ObserverSet().RemoveObserver(observer2);
    EXPECT_FALSE(notifier->ObserverSet().HasObserver(observer2));
  });
}

}  // namespace blink
