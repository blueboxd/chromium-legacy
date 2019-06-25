// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DisplayLockSuspendedHandle;
class Element;
class DisplayLockOptions;
class DisplayLockScopedLogger;
class TaskHandle;
class CORE_EXPORT DisplayLockContext final
    : public ScriptWrappable,
      public ActiveScriptWrappable<DisplayLockContext>,
      public ContextLifecycleObserver,
      public LocalFrameView::LifecycleNotificationObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DisplayLockContext);
  USING_PRE_FINALIZER(DisplayLockContext, Dispose);

 public:
  // Determines what type of budget to use. This can be overridden via
  // DisplayLockContext::SetBudgetType().
  // - kDoNotYield:
  //     This will effectively do all of the lifecycle phases when we're
  //     committing. That is, this is the "no budget" option.
  // - kStrictYieldBetweenLifecyclePhases:
  //     This type always yields between each lifecycle phase, even if that
  //     phase was quick (note that it still skips phases that don't need any
  //     updates).
  // - kYieldBetweenLifecyclePhases:
  //     This type will only yield between lifecycle phases (not in the middle
  //     of one). However, if there is sufficient time left (TODO(vmpstr):
  //     define this), then it will continue on to the next lifecycle phase.
  enum class BudgetType {
    kDoNotYield,
    kStrictYieldBetweenLifecyclePhases,
    kYieldBetweenLifecyclePhases,
    kDefault = kYieldBetweenLifecyclePhases
  };

  // The current state of the lock. Note that the order of these matters.
  enum State {
    kLocked,
    kUpdating,
    kCommitting,
    kUnlocked,
  };

  // The type of style that was blocked by this display lock.
  enum StyleType {
    kStyleUpdateNotRequired,
    kStyleUpdateSelf,
    kStyleUpdateChildren,
    kStyleUpdateDescendants
  };

  // See GetScopedForcedUpdate() for description.
  class CORE_EXPORT ScopedForcedUpdate {
    DISALLOW_NEW();

   public:
    ScopedForcedUpdate(ScopedForcedUpdate&&);
    ~ScopedForcedUpdate();

   private:
    friend class DisplayLockContext;

    ScopedForcedUpdate(DisplayLockContext*);

    UntracedMember<DisplayLockContext> context_ = nullptr;
  };

  DisplayLockContext(Element*, ExecutionContext*);
  ~DisplayLockContext() override;

  // GC functions.
  void Trace(blink::Visitor*) override;
  void Dispose();

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  // ActiveScriptWrappable overrides. This keeps the context alive as long as we
  // have an element and we're not unlocked.
  bool HasPendingActivity() const final;

  // JavaScript interface implementation. See display_lock_context.idl for
  // description.
  ScriptPromise acquire(ScriptState*, DisplayLockOptions*);
  ScriptPromise update(ScriptState*);
  ScriptPromise commit(ScriptState*);
  ScriptPromise updateAndCommit(ScriptState*);

  enum LifecycleTarget { kSelf, kChildren };

  // Lifecycle observation / state functions.
  bool ShouldStyle(LifecycleTarget) const;
  void DidStyle(LifecycleTarget);
  bool ShouldLayout(LifecycleTarget) const;
  void DidLayout(LifecycleTarget);
  bool ShouldPrePaint(LifecycleTarget) const;
  void DidPrePaint(LifecycleTarget);
  bool ShouldPaint(LifecycleTarget) const;
  void DidPaint(LifecycleTarget);

  // Returns true if the last style recalc traversal was blocked at this
  // element, either for itself, its children or its descendants.
  bool StyleTraversalWasBlocked() {
    return blocked_style_traversal_type_ != kStyleUpdateNotRequired;
  }

  // Returns true if the contents of the associated element should be visible
  // from and activatable by find-in-page, tab order, anchor links, etc.
  bool IsActivatable() const;

  // Trigger commit because of activation from tab order, url fragment,
  // find-in-page, etc.
  void CommitForActivation();

  bool ShouldCommitForActivation() const;

  // Returns true if this lock is locked. Note from the outside perspective, the
  // lock is locked any time the state is not kUnlocked or kCommitting.
  bool IsLocked() const { return state_ != kUnlocked && state_ != kCommitting; }

  // Called when the layout tree is attached. This is used to verify
  // containment.
  void DidAttachLayoutTree();

  // Returns a ScopedForcedUpdate object which for the duration of its lifetime
  // will allow updates to happen on this element's subtree. For the element
  // itself, the frame rect will still be the same as at the time the lock was
  // acquired. Only one ScopedForcedUpdate can be retrieved from the same
  // context at a time.
  ScopedForcedUpdate GetScopedForcedUpdate();

  // This is called when the element with which this context is associated is
  // moved to a new document. Used to listen to the lifecycle update from the
  // right document's view.
  void DidMoveToNewDocument(Document& old_document);

  void AddToWhitespaceReattachSet(Element& element);

  // LifecycleNotificationObserver overrides.
  void WillStartLifecycleUpdate(const LocalFrameView&) override;
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  // Inform the display lock that it prevented a style change. This is used to
  // invalidate style when we need to update it in the future.
  void NotifyStyleRecalcWasBlocked(StyleType type) {
    blocked_style_traversal_type_ =
        std::max(blocked_style_traversal_type_, type);
  }

  void NotifyReattachLayoutTreeWasBlocked() {
    reattach_layout_tree_was_blocked_ = true;
  }

  void NotifyChildLayoutWasBlocked() { child_layout_was_blocked_ = true; }

  // Inform the display lock that it needs a graphics layer collection when it
  // needs to paint.
  void NotifyNeedsGraphicsLayerCollection() {
    needs_graphics_layer_collection_ = true;
  }

  // Notify this element will be disconnected.
  void NotifyWillDisconnect();

  void SetNeedsPrePaintSubtreeWalk(
      bool needs_effective_allowed_touch_action_update) {
    needs_effective_allowed_touch_action_update_ =
        needs_effective_allowed_touch_action_update;
    needs_prepaint_subtree_walk_ = true;
  }

  LayoutUnit GetLockedContentLogicalWidth() const {
    return is_horizontal_writing_mode_ ? locked_content_logical_size_.Width()
                                       : locked_content_logical_size_.Height();
  }
  LayoutUnit GetLockedContentLogicalHeight() const {
    return is_horizontal_writing_mode_ ? locked_content_logical_size_.Height()
                                       : locked_content_logical_size_.Width();
  }

 private:
  friend class DisplayLockContextTest;
  friend class DisplayLockBudgetTest;
  friend class DisplayLockSuspendedHandle;
  friend class DisplayLockBudget;

  class StateChangeHelper {
    DISALLOW_NEW();

   public:
    explicit StateChangeHelper(DisplayLockContext*);

    operator State() const { return state_; }
    StateChangeHelper& operator=(State);
    void UpdateActivationBlockingCount(bool old_activatable,
                                       bool new_activatable);

   private:
    State state_ = kUnlocked;
    UntracedMember<DisplayLockContext> context_;
  };

  // Initiate a commit.
  void StartCommit();
  // Initiate an update.
  void StartUpdateIfNeeded();

  // Marks ancestors of elements in |whitespace_reattach_set_| with
  // ChildNeedsReattachLayoutTree and clears the set.
  void MarkElementsForWhitespaceReattachment();

  // The following functions propagate dirty bits from the locked element up to
  // the ancestors in order to be reached, and update dirty bits for the element
  // as well if needed. They return true if the element or its subtree were
  // dirty, and false otherwise.
  bool MarkForStyleRecalcIfNeeded();
  bool MarkForLayoutIfNeeded();
  bool MarkAncestorsForPrePaintIfNeeded();
  bool MarkPaintLayerNeedsRepaint();

  bool IsElementDirtyForStyleRecalc() const;
  bool IsElementDirtyForLayout() const;
  bool IsElementDirtyForPrePaint() const;

  // When ScopedForcedUpdate is destroyed, it calls this function. See
  // GetScopedForcedUpdate() for more information.
  void NotifyForcedUpdateScopeEnded();

  // Creates a new update budget based on the BudgetType::kDefault enum. In
  // other words, it will create a budget of that type.
  // TODO(vmpstr): In tests, we will probably switch the value to test other
  // budgets. As well, this makes it easier to change the budget right in the
  // enum definitions.
  std::unique_ptr<DisplayLockBudget> CreateNewBudget();

  // Helper to schedule an animation to delay lifecycle updates for the next
  // frame.
  void ScheduleAnimation();

  // Timeout implementation. When the lock is acquired, we kick off a timeout
  // task that will trigger a commit (which can be canceled by other calls to
  // schedule or by a call to commit). Note that calling RescheduleTimeoutTask()
  // will cancel any previously scheduled task.
  void RescheduleTimeoutTask(double delay);
  void CancelTimeoutTask();
  void TriggerTimeout();

  // Helper functions to resolve the update/commit promises.
  enum ResolverState { kResolve, kReject, kDetach };
  void MakeResolver(ScriptState*, Member<ScriptPromiseResolver>*);
  bool HasResolver();
  void FinishUpdateResolver(ResolverState, const char* reject_reason = nullptr);
  void FinishCommitResolver(ResolverState, const char* reject_reason = nullptr);
  void FinishAcquireResolver(ResolverState,
                             const char* reject_reason = nullptr);
  void FinishResolver(Member<ScriptPromiseResolver>*,
                      ResolverState,
                      const char* reject_reason);

  // Checks whether we should force unlock the lock (due to not meeting
  // containment/display requirements), returns a string from rejection_names
  // if we should, nullptr if not. Note that this can only be called if the
  // style is clean. It checks the layout object if it exists. Otherwise,
  // falls back to checking computed style.
  const char* ShouldForceUnlock() const;

  // Unlocks the lock if the element doesn't meet requirements
  // (containment/display type). Returns true if we did unlock.
  bool ForceUnlockIfNeeded();

  // Returns true if the element is connected to a document that has a view.
  // If we're not connected,  or if we're connected but the document doesn't
  // have a view (e.g. templates) we shouldn't do style calculations etc and
  // when acquiring this lock should immediately resolve the acquire promise.
  bool ConnectedToView() const;

  bool ShouldPerformUpdatePhase(DisplayLockBudget::Phase phase) const;

  // During an attempt to commit, clean up state and reject pending resolver
  // promises if the lock is not connected to the tree.
  bool CleanupAndRejectCommitIfNotConnected();

  std::unique_ptr<DisplayLockBudget> update_budget_;

  Member<ScriptPromiseResolver> commit_resolver_;
  Member<ScriptPromiseResolver> update_resolver_;
  Member<ScriptPromiseResolver> acquire_resolver_;
  WeakMember<Element> element_;
  WeakMember<Document> document_;

  // See StyleEngine's |whitespace_reattach_set_|.
  // Set of elements that had at least one rendered children removed
  // since its last lifecycle update. For such elements that are located
  // in a locked subtree, we save it here instead of the global set in
  // StyleEngine because we don't want to accidentally mark elements
  // in a locked subtree for layout tree reattachment before we did
  // style recalc on them.
  HeapHashSet<Member<Element>> whitespace_reattach_set_;

  StateChangeHelper state_;
  LayoutSize locked_content_logical_size_;

  bool update_forced_ = false;
  bool activatable_ = false;

  bool is_locked_after_connect_ = false;
  StyleType blocked_style_traversal_type_ = kStyleUpdateNotRequired;
  // Signifies whether we've blocked a layout tree reattachment on |element_|'s
  // descendants or not, so that we can mark |element_| for reattachment when
  // needed.
  bool reattach_layout_tree_was_blocked_ = false;

  bool needs_effective_allowed_touch_action_update_ = false;
  bool needs_prepaint_subtree_walk_ = false;
  bool is_horizontal_writing_mode_ = true;
  bool needs_graphics_layer_collection_ = false;
  // Will be true if child traversal was blocked on a previous layout run on the
  // locked element. We need to keep track of this to ensure that on the next
  // layout run where the descendants of the locked element are allowed to be
  // traversed into, we will traverse to the children of the locked element.
  bool child_layout_was_blocked_ = false;

  TaskHandle timeout_task_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_DISPLAY_LOCK_CONTEXT_H_
