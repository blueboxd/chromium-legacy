// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Static utility class for display-locking related helpers.
class CORE_EXPORT DisplayLockUtilities {
  STATIC_ONLY(DisplayLockUtilities);

 public:
  // This class forces updates on display locks from the given node up the
  // ancestor chain until the local frame root.
  class CORE_EXPORT ScopedChainForcedUpdate {
    DISALLOW_COPY_AND_ASSIGN(ScopedChainForcedUpdate);

   public:
    explicit ScopedChainForcedUpdate(const Node* node,
                                     bool include_self = false);
    ~ScopedChainForcedUpdate() = default;

    void CreateParentFrameScopeIfNeeded(const Node* node);

   private:
    Vector<DisplayLockContext::ScopedForcedUpdate> scoped_update_forced_list_;
    std::unique_ptr<ScopedChainForcedUpdate> parent_frame_scope_;
  };
  // Activates all the nodes within a find-in-page match |range|.
  // Returns true if at least one node gets activated.
  // See: http://bit.ly/2RXULVi, "beforeactivate Event" part.
  static bool ActivateFindInPageMatchRangeIfNeeded(
      const EphemeralRangeInFlatTree& range);

  // Activates all locked nodes in |range| that are activatable and doesn't
  // have user-select:none. Returns true if we activated at least one node.
  static bool ActivateSelectionRangeIfNeeded(
      const EphemeralRangeInFlatTree& range);

  // Returns activatable-locked inclusive ancestors of |node|.
  // Note that this function will return an empty list if |node| is inside a
  // non-activatable locked subtree (e.g. at least one ancestor is not
  // activatable-locked).
  static const HeapVector<Member<Element>> ActivatableLockedInclusiveAncestors(
      const Node& node,
      DisplayLockActivationReason reason);

  // Returns the nearest inclusive ancestor of |node| that is display locked.
  static const Element* NearestLockedInclusiveAncestor(const Node& node);
  static Element* NearestLockedInclusiveAncestor(Node& node);

  // Returns the nearest non-inclusive ancestor of |node| that is display
  // locked.
  static Element* NearestLockedExclusiveAncestor(const Node& node);

  // Returns the highest inclusive ancestor of |node| that is display locked.
  static Element* HighestLockedInclusiveAncestor(const Node& node);

  // Returns the highest exclusive ancestor of |node| that is display locked.
  static Element* HighestLockedExclusiveAncestor(const Node& node);

  // LayoutObject versions of the NearestLocked* ancestor functions.
  static Element* NearestLockedInclusiveAncestor(const LayoutObject& object);
  static Element* NearestLockedExclusiveAncestor(const LayoutObject& object);

  // Returns true if |node| is not in a locked subtree, or if it's possible to
  // activate all of the locked ancestors for |activation_reason|.
  static bool IsInUnlockedOrActivatableSubtree(
      const Node& node,
      DisplayLockActivationReason activation_reason =
          DisplayLockActivationReason::kAny);

  // Returns true if |node| is in a locked subtree, and at least one of its
  // locked ancestors can't be activated with |activation_reason|. In other
  // words, this node should be treated as if it's not in the tree for
  // |activation_reason|.
  static bool ShouldIgnoreNodeDueToDisplayLock(
      const Node& node,
      DisplayLockActivationReason activation_reason) {
    return !IsInUnlockedOrActivatableSubtree(node, activation_reason);
  }

  // Returns true if the element is in a locked subtree (or is self-locked with
  // no self-updates). This crosses frames while navigating the ancestor chain.
  static bool IsInLockedSubtreeCrossingFrames(const Node& node);

  // Called when the focused element changes. These functions update locks to
  // ensure that focused element ancestors remain unlocked for 'auto' state.
  static void ElementLostFocus(Element*);
  static void ElementGainedFocus(Element*);

  static void SelectionChanged(const EphemeralRangeInFlatTree& old_selection,
                               const EphemeralRangeInFlatTree& new_selection);
  static void SelectionRemovedFromDocument(Document& document);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_UTILITIES_H_
