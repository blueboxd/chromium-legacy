// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/mutation_observer_notifier.h"

#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

MutationObserverNotifier::MutationObserverNotifier() {}

void MutationObserverNotifier::EnqueueSlotChange(HTMLSlotElement& slot) {
  EnqueueMicrotaskIfNecessary();
  active_slot_change_list_.push_back(&slot);
}

void MutationObserverNotifier::CleanSlotChangeList(Document& document) {
  SlotChangeList kept;
  kept.ReserveCapacity(active_slot_change_list_.size());
  for (auto& slot : active_slot_change_list_) {
    if (slot->GetDocument() != document)
      kept.push_back(slot);
  }
  active_slot_change_list_.swap(kept);
}

void MutationObserverNotifier::ActivateObserver(MutationObserver* observer) {
  EnqueueMicrotaskIfNecessary();
  active_mutation_observers_.insert(observer);
}

void MutationObserverNotifier::EnqueueMicrotaskIfNecessary() {
  if (microtask_enqueued_) {
    return;
  }

  // If this is called before inserting the observer or slot, they should both
  // be empty.
  DCHECK(active_mutation_observers_.IsEmpty());
  DCHECK(active_slot_change_list_.IsEmpty());

  microtask_enqueued_ = true;
  Microtask::EnqueueMicrotask(WTF::Bind(
      &MutationObserverNotifier::DeliverMutations, WrapPersistent(this)));
}

struct MutationObserverNotifier::ObserverLessThan {
  bool operator()(const Member<MutationObserver>& lhs,
                  const Member<MutationObserver>& rhs) {
    return lhs->priority() < rhs->priority();
  }
};

void MutationObserverNotifier::DeliverMutations() {
  // These steps are defined in DOM Standard's "notify mutation observers".
  // https://dom.spec.whatwg.org/#notify-mutation-observers
  DCHECK(IsMainThread());

  microtask_enqueued_ = false;

  MutationObserverVector observers;
  CopyToVector(active_mutation_observers_, observers);
  active_mutation_observers_.clear();

  SlotChangeList slots;
  slots.swap(active_slot_change_list_);
  for (const auto& slot : slots)
    slot->ClearSlotChangeEventEnqueued();

  std::sort(observers.begin(), observers.end(), ObserverLessThan());
  for (const auto& observer : observers) {
    if (!observer->GetExecutionContext()) {
      // The observer's execution context is already gone, as active observers
      // intentionally do not hold their execution context. Do nothing then.
      continue;
    }

    if (observer->ShouldBeSuspended())
      suspended_mutation_observers_.insert(observer);
    else
      observer->Deliver();
  }
  for (const auto& slot : slots)
    slot->DispatchSlotChangeEvent();
}

void MutationObserverNotifier::ResumeSuspendedObservers() {
  DCHECK(IsMainThread());
  if (suspended_mutation_observers_.IsEmpty())
    return;

  MutationObserverVector suspended;
  CopyToVector(suspended_mutation_observers_, suspended);
  for (const auto& observer : suspended) {
    if (!observer->ShouldBeSuspended()) {
      suspended_mutation_observers_.erase(observer);
      ActivateObserver(observer);
    }
  }
}

unsigned MutationObserverNotifier::NextObserverPriority() {
  return ++last_observer_priority_;
}

void MutationObserverNotifier::Trace(Visitor* visitor) {
  visitor->Trace(active_mutation_observers_);
  visitor->Trace(suspended_mutation_observers_);
  visitor->Trace(active_slot_change_list_);
}

}  // namespace blink
