// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_DOCUMENT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_DOCUMENT_STATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
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
class CORE_EXPORT DisplayLockDocumentState final
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
  // activation (i.e. content-visibility: hidden locks).
  void IncrementDisplayLockBlockingAllActivation();
  void DecrementDisplayLockBlockingAllActivation();
  int DisplayLockBlockingAllActivationCount() const;

  // Register the given element for intersection observation. Used for detecting
  // viewport intersections for content-visibility: auto locks.
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

  // This is called when the ScopedChainForcedUpdate is created or destroyed.
  // This is used to ensure that we can create new locks that are immediately
  // forced by the existing forced scope.
  //
  // Consider the situation A -> B -> C, where C is the child node which is the
  // target of the forced lock (the parameter passed here), and B is its parent
  // and A is its grandparent. Suppose that A and B have locks, but since style
  // was blocked by A, B's lock has not been created yet. When we force the
  // update from C we call `NotifyNodeForced()`, and A's lock is forced by the
  // given ScopedChainForcedUpdate. Then we process the style and while
  // processing B's style, we find that there is a new lock there. This lock
  // needs to be forced immediately, since it is in the ancestor chain of C.
  // This is done by calling `ForceLockIfNeeded()` below, which adds B's scope
  // to the chain. At the end of the scope, everything is un-forced and
  // `EndNodeForcedScope()` is called to clean up state.
  //
  // Note that there can only be one scope created at a time, so we don't keep
  // track of more than one of these scopes. This is enforced by private access
  // modifier + friends, as well as DCHECKs.
  void BeginNodeForcedScope(
      const Node* node,
      bool self_was_forced,
      DisplayLockUtilities::ScopedChainForcedUpdate* scope);
  void EndNodeForcedScope(DisplayLockUtilities::ScopedChainForcedUpdate* scope);

  // Forces the lock on the given element, if it isn't yet forced but appears on
  // the ancestor chain for the forced element (which was set via
  // `BeginNodeForcedScope()`).
  void ForceLockIfNeeded(Element*);

 private:
  struct ForcedNodeInfo {
    ForcedNodeInfo(const Node* node,
                   bool self_forced,
                   DisplayLockUtilities::ScopedChainForcedUpdate* scope)
        : node(node), self_forced(self_forced), scope(scope) {}

    // Since this is created via a Scoped stack-only object, we know that GC
    // won't run so this is safe to store as an untraced member.
    UntracedMember<const Node> node;
    bool self_forced;
    DisplayLockUtilities::ScopedChainForcedUpdate* scope;
  };

  IntersectionObserver& EnsureIntersectionObserver();

  void ProcessDisplayLockActivationObservation(
      const HeapVector<Member<IntersectionObserverEntry>>&);

  void ForceLockIfNeededForInfo(Element*, ForcedNodeInfo*);

  // Note that since this class is owned by the document, it is important not to
  // take a strong reference for the backpointer.
  WeakMember<Document> document_ = nullptr;

  Member<IntersectionObserver> intersection_observer_ = nullptr;
  HeapHashSet<WeakMember<DisplayLockContext>> display_lock_contexts_;

  int locked_display_lock_count_ = 0;
  int display_lock_blocking_all_activation_count_ = 0;

  // If greater than 0, then the activatable locks are forced.
  int activatable_display_locks_forced_ = 0;

  // Contains all of the currently forced node infos, each of which represents
  // the node that caused the scope to be created.
  Vector<ForcedNodeInfo> forced_node_info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_DOCUMENT_STATE_H_
