// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_DOCUMENT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_DOCUMENT_STATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class DisplayLockContext;
class Document;
class Element;
class IntersectionObserver;
class IntersectionObserverEntry;

// This class is responsible for keeping document level state for the display
// locking feature.
class CORE_EXPORT DisplayLockDocumentState
    : public GarbageCollected<DisplayLockDocumentState> {
 public:
  explicit DisplayLockDocumentState(Document* document);

  // GC.
  void Trace(Visitor*);

  // Registers a display lock context with the state. This is used to force all
  // activatable locks.
  void AddDisplayLockContext(DisplayLockContext*);
  void RemoveDisplayLockContext(DisplayLockContext*);
  int DisplayLockCount() const;

  // Bookkeeping: the count of all locked display locks.
  void AddLockedDisplayLock();
  void RemoveLockedDisplayLock();
  int LockedDisplayLockCount() const;

  // Bookkeeping: the count of all locked display locks which block all
  // activation (i.e. subtree-visibility: hidden locks).
  void IncrementDisplayLockBlockingAllActivation();
  void DecrementDisplayLockBlockingAllActivation();
  int DisplayLockBlockingAllActivationCount() const;

  // Register the given element for intersection observation. Used for detecting
  // viewport intersections for subtree-visibility: auto locks.
  void RegisterDisplayLockActivationObservation(Element*);
  void UnregisterDisplayLockActivationObservation(Element*);

  // Returns true if all activatable locks have been forced.
  bool ActivatableDisplayLocksForced() const;

  class ScopedForceActivatableDisplayLocks {
    STACK_ALLOCATED();

   public:
    ScopedForceActivatableDisplayLocks(ScopedForceActivatableDisplayLocks&&);
    ~ScopedForceActivatableDisplayLocks();

    ScopedForceActivatableDisplayLocks& operator=(
        ScopedForceActivatableDisplayLocks&&);

   private:
    friend DisplayLockDocumentState;
    explicit ScopedForceActivatableDisplayLocks(DisplayLockDocumentState*);

    DisplayLockDocumentState* state_;
  };

  ScopedForceActivatableDisplayLocks GetScopedForceActivatableLocks();

  // Notify the display locks that selection was removed.
  void NotifySelectionRemoved();

 private:
  IntersectionObserver& EnsureIntersectionObserver();

  void ProcessDisplayLockActivationObservation(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  // Note that since this class is owned by the document, it is important not to
  // take a strong reference for the backpointer.
  WeakMember<Document> document_ = nullptr;

  Member<IntersectionObserver> intersection_observer_ = nullptr;
  HeapHashSet<WeakMember<DisplayLockContext>> display_lock_contexts_;

  int locked_display_lock_count_ = 0;
  int display_lock_blocking_all_activation_count_ = 0;

  // If greater than 0, then the activatable locks are forced.
  int activatable_display_locks_forced_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_DOCUMENT_STATE_H_
