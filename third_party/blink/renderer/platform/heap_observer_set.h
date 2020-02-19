// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_SET_H_

#include "base/auto_reset.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// A set of observers. Observers are not retained.
template <class ObserverType>
class PLATFORM_EXPORT HeapObserverSet {
  DISALLOW_NEW();

 public:
  // Add an observer to this list. An observer should not be added to the same
  // list more than once. Observers cannot be added while iterating.
  void AddObserver(ObserverType* observer) {
    CHECK(!IsIteratingOverObservers());
    DCHECK(!HasObserver(observer));
    observers_.insert(observer);
  }

  // Removes the given observer from this list. Does nothing if this observer is
  // not in this list. Observers can be removed while iterating.
  void RemoveObserver(ObserverType* observer) {
    if (removed_observers_)
      removed_observers_->insert(observer);
    else
      observers_.erase(observer);
  }

  // Determine whether a particular observer is in the list.
  bool HasObserver(ObserverType* observer) const {
    if (removed_observers_ && removed_observers_->Contains(observer)) {
      return false;
    }
    return observers_.Contains(observer);
  }

  // Returns true if the list is being iterated over.
  bool IsIteratingOverObservers() const { return !!removed_observers_; }

  // Removes all the observers from this list.
  void Clear() {
    // Clearing while iterating is technically possible but disallowed as it is
    // unusual.
    CHECK(!IsIteratingOverObservers());
    observers_.clear();
  }

  // Safely iterate over the registered lifecycle observers. Order is not
  // stable.
  //
  // Adding observers is not allowed during iteration. The callable
  // will only be called synchronously inside ForEachObserver(). If an observer
  // is removed before its turn, it will not be called.
  //
  // Sample usage:
  //     ForEachObserver([](ObserverType* observer) {
  //       observer->SomeMethod();
  //     });
  template <typename ForEachCallable>
  void ForEachObserver(const ForEachCallable& callable) const {
    CHECK(!IsIteratingOverObservers());
    base::AutoReset<Member<HeapHashSet<Member<ObserverType>>>> scope(
        &removed_observers_,
        MakeGarbageCollected<HeapHashSet<Member<ObserverType>>>());
    for (ObserverType* observer : observers_) {
      if (removed_observers_->Contains(observer)) {
        continue;
      }
      callable(observer);
    }
    for (ObserverType* observer : *removed_observers_) {
      observers_.erase(observer);
    }
  }

  void Trace(Visitor* visitor) {
    visitor->Trace(observers_);
    visitor->Trace(removed_observers_);
  }

 private:
  mutable HeapHashSet<WeakMember<ObserverType>> observers_;
  mutable Member<HeapHashSet<Member<ObserverType>>> removed_observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_OBSERVER_SET_H_
