// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_MUTATION_OBSERVER_NOTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_MUTATION_OBSERVER_NOTIFIER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;
class MutationObserver;
class HTMLSlotElement;

// In charge of mutation observer related state that should be managed per
// WindowAgent.
class MutationObserverNotifier final
    : public GarbageCollectedFinalized<MutationObserverNotifier> {
 public:
  MutationObserverNotifier();

  void Trace(Visitor*);

  void EnqueueSlotChange(HTMLSlotElement&);
  void CleanSlotChangeList(Document&);
  void ActivateObserver(MutationObserver*);
  void ResumeSuspendedObservers();
  // Returns the priority that should be assigned to the next MutationObserver.
  unsigned NextObserverPriority();

  // Callback called to deliver the accumulated mutations.
  void DeliverMutations();

 private:
  struct ObserverLessThan;

  // Schedules the DeliverMutations() callback if necessary.
  void EnqueueMicrotaskIfNecessary();

  using MutationObserverSet = HeapHashSet<Member<MutationObserver>>;
  using SlotChangeList = HeapVector<Member<HTMLSlotElement>>;
  using MutationObserverVector = HeapVector<Member<MutationObserver>>;

  bool microtask_enqueued_ = false;
  MutationObserverSet active_mutation_observers_;
  MutationObserverSet suspended_mutation_observers_;
  SlotChangeList active_slot_change_list_;
  unsigned last_observer_priority_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_MUTATION_OBSERVER_NOTIFIER_H_
