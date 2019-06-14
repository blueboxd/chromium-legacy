// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_options.h"
#include "third_party/blink/renderer/core/display_lock/strict_yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/unyielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {
// The default timeout for the lock if a timeout is not specified. Defaults to 1
// sec.
double kDefaultLockTimeoutMs = 1000.;

namespace rejection_names {
const char* kElementIsUnlocked = "Element is unlocked.";
const char* kExecutionContextDestroyed = "Execution context destroyed.";
const char* kContainmentNotSatisfied =
    "Containment requirement is not satisfied.";
const char* kUnsupportedDisplay =
    "Element has unsupported display type (display: contents).";
const char* kElementIsDisconnected = "Element is disconnected.";
const char* kLockCommitted = "Lock commit was requested.";
const char* kInvalidOptions = "Invalid options.";
}  // namespace rejection_names

// Helper function to convert a display locking state to a string. Used in
// traces.
std::string StateToString(DisplayLockContext::State state) {
  switch (state) {
    case DisplayLockContext::kLocked:
      return "kLocked";
    case DisplayLockContext::kUpdating:
      return "kUpdating";
    case DisplayLockContext::kCommitting:
      return "kCommitting";
    case DisplayLockContext::kUnlocked:
      return "kUnlocked";
  }
  return "";
}

// Helper function that returns an immediately rejected promise.
ScriptPromise GetRejectedPromise(ScriptState* script_state,
                                 const char* rejection_reason) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotAllowedError, rejection_reason));
  return promise;
}

// Helper function that returns an immediately resolved promise.
ScriptPromise GetResolvedPromise(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();
  resolver->Resolve();
  return promise;
}

base::Optional<LayoutSize> ParseAndVerifySize(const Vector<double>& size) {
  if (size.size() != 2 || size[0] < 0 || size[1] < 0)
    return {};
  return LayoutSize(static_cast<float>(size[0]), static_cast<float>(size[1]));
}

}  // namespace

DisplayLockContext::DisplayLockContext(Element* element,
                                       ExecutionContext* context)
    : ContextLifecycleObserver(context),
      element_(element),
      document_(&element_->GetDocument()),
      state_(this) {}

DisplayLockContext::~DisplayLockContext() {
  DCHECK_EQ(state_, kUnlocked);
}

void DisplayLockContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(update_resolver_);
  visitor->Trace(commit_resolver_);
  visitor->Trace(acquire_resolver_);
  visitor->Trace(element_);
  visitor->Trace(document_);
  visitor->Trace(whitespace_reattach_set_);
  ScriptWrappable::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void DisplayLockContext::Dispose() {
  // Note that if we have any resolvers at dispose time, then it's too late to
  // reject the promise, since we are not allowed to create new strong
  // references to objects already set for destruction (and rejecting would do
  // this since the rejection has to be deferred). We need to detach instead.
  // TODO(vmpstr): See if there is another earlier time we can detect that we're
  // going to be disposed.
  FinishUpdateResolver(kDetach);
  FinishCommitResolver(kDetach);
  FinishAcquireResolver(kDetach);
  CancelTimeoutTask();
  state_ = kUnlocked;
}

void DisplayLockContext::ContextDestroyed(ExecutionContext*) {
  FinishUpdateResolver(kReject, rejection_names::kExecutionContextDestroyed);
  FinishCommitResolver(kReject, rejection_names::kExecutionContextDestroyed);
  FinishAcquireResolver(kReject, rejection_names::kExecutionContextDestroyed);
  CancelTimeoutTask();
  state_ = kUnlocked;
}

bool DisplayLockContext::HasPendingActivity() const {
  // If we're locked or doing any work and have an element, then we should stay
  // alive. If the element is gone, then there is no reason for the context to
  // remain. Also, if we're unlocked we're essentially "idle" so GC can clean us
  // up. If the script needs the context, the element would create a new one.
  return element_ && state_ != kUnlocked;
}

ScriptPromise DisplayLockContext::acquire(ScriptState* script_state,
                                          DisplayLockOptions* options) {
  TRACE_EVENT0("blink", "DisplayLockContext::acquire()");
  double timeout_ms = (options && options->hasTimeout())
                          ? options->timeout()
                          : kDefaultLockTimeoutMs;

  if (IsLocked()) {
    // If we're locked, the activatable flag might change the activation
    // blocking lock count. If we're not locked, the activation blocking lock
    // count will be updated when we changed the state.
    state_.UpdateActivationBlockingCount(activatable_,
                                         options && options->activatable());
  }
  activatable_ = options && options->activatable();

  // If we're acquiring something that isn't currently locked, we need to recalc
  // the layout size. Otherwise if we're re-acquiring, we only need to recalc if
  // the locked size has changed.
  bool should_recalc_layout_size = !IsLocked();
  if (options && options->hasSize()) {
    auto parsed_size = ParseAndVerifySize(options->size());
    if (!parsed_size)
      return GetRejectedPromise(script_state, rejection_names::kInvalidOptions);
    if (locked_content_logical_size_ != *parsed_size)
      should_recalc_layout_size = true;
    locked_content_logical_size_ = *parsed_size;

  } else {
    if (locked_content_logical_size_ != LayoutSize())
      should_recalc_layout_size = true;
    locked_content_logical_size_ = LayoutSize();
  }

  // We always reschedule a timeout task even if we're not starting a new
  // acquire. The reason for this is that the last acquire dictates the timeout
  // interval. Note that the following call cancels any existing timeout tasks.
  RescheduleTimeoutTask(timeout_ms);

  if (should_recalc_layout_size && ConnectedToView()) {
    if (auto* layout_object = element_->GetLayoutObject()) {
      layout_object->SetNeedsLayoutAndPrefWidthsRecalc(
          layout_invalidation_reason::kDisplayLock);
    }
    ScheduleAnimation();
  }

  if (acquire_resolver_)
    return acquire_resolver_->Promise();

  // At this point, we updated state based on the options so if we're locked,
  // resolve.
  if (IsLocked())
    return GetResolvedPromise(script_state);

  // We might be committing (which means we're already unlocked for the purposes
  // of the above check. We resolve the commit, since rejecting it seems wrong.
  // TODO(vmpstr): We could also reject this, but then script would have no way
  // to avoid this rejection (since at the time acquire is called, locked
  // property is false).
  FinishCommitResolver(kResolve);

  update_budget_.reset();
  state_ = kLocked;

  // If we're already connected then we need to ensure that we update our layout
  // size based on the options and we have cleared the painted output.
  if (ConnectedToView()) {
    if (auto* reason = ShouldForceUnlock()) {
      // The element has up-to-date style and doesn't satisfy the containment
      // or display type requirement, so we unlock and reject now.
      // If the style needs recalc we would instead check the requirements after
      // style recalc for this element.
      DCHECK(!element_->NeedsStyleRecalc());
      CancelTimeoutTask();
      state_ = kUnlocked;
      return GetRejectedPromise(script_state, reason);
    }

    MakeResolver(script_state, &acquire_resolver_);
    is_horizontal_writing_mode_ = true;
    if (auto* layout_object = element_->GetLayoutObject()) {
      is_horizontal_writing_mode_ = layout_object->IsHorizontalWritingMode();
      // GraphicsLayer collection would normally skip layers if paint is blocked
      // by display-locking (see: CollectDrawableLayersForLayerListRecursively
      // in LocalFrameView). However, if we don't trigger this collection, then
      // we might use the cached result instead. In order to ensure we skip the
      // newly locked layers, we need to set |need_graphics_layer_collection_|
      // before marking the layer for repaint.
      needs_graphics_layer_collection_ = true;
      MarkPaintLayerNeedsRepaint();
    }
    // TODO(vmpstr): This needs to be set after invalidation above, since we
    // want the object to layout once. After the changes to separate self and
    // child layout, this would no longer be required and we can set the
    // container as locked earlier.
    state_ = kLocked;
    return acquire_resolver_->Promise();
  }

  // Otherwise (if we're not connected), resolve immediately.
  return GetResolvedPromise(script_state);
}

ScriptPromise DisplayLockContext::update(ScriptState* script_state) {
  TRACE_EVENT0("blink", "DisplayLockContext::update()");
  // Reject if we're unlocked or disconnected.
  if (state_ == kUnlocked || !ConnectedToView()) {
    return GetRejectedPromise(script_state,
                              rejection_names::kElementIsUnlocked);
  }

  // If we have a resolver, then we're at least updating already, just return
  // the same promise.
  if (update_resolver_) {
    DCHECK(state_ == kUpdating || state_ == kCommitting) << state_;
    return update_resolver_->Promise();
  }

  MakeResolver(script_state, &update_resolver_);
  StartUpdateIfNeeded();
  return update_resolver_->Promise();
}

bool DisplayLockContext::CleanupAndRejectCommitIfNotConnected() {
  // If we don't have an element or we're not connected, then the process of
  // committing is the same as just unlocking the element. Early out if
  // those conditions *don't* hold.
  if (element_ && ConnectedToView())
    return false;

  state_ = kUnlocked;
  update_budget_.reset();
  // Note that we reject the update, but resolve the commit.
  FinishUpdateResolver(kReject, rejection_names::kElementIsDisconnected);
  FinishCommitResolver(kResolve);
  return true;
}

ScriptPromise DisplayLockContext::commit(ScriptState* script_state) {
  TRACE_EVENT0("blink", "DisplayLockContext::commit()");
  // Resolve if we're already unlocked.
  if (state_ == kUnlocked)
    return GetResolvedPromise(script_state);

  // If we're already committing and it's not because of activation,
  // we would already have the commit promise and should just return it.
  if (commit_resolver_) {
    // When we perform an updateAndCommit(), we schedule a StartCommit() via a
    // task. While that task is in flight, our state could still be locked. It,
    // of course, can also be updating or committing if that's what we are
    // currently doing. So the only state we can't be in is kUnlocked.
    DCHECK(state_ != kUnlocked);
    return commit_resolver_->Promise();
  }

  if (CleanupAndRejectCommitIfNotConnected())
    return GetResolvedPromise(script_state);

  // Note that we don't resolve the update promise here, since it should still
  // finish updating before resolution. That is, calling update() and commit()
  // together will still wait until the lifecycle is clean before resolving any
  // of the promises.

  MakeResolver(script_state, &commit_resolver_);
  auto promise = commit_resolver_->Promise();
  // It's possible we are already committing due to activation. If not, we
  // should start the commit.
  if (state_ != kCommitting)
    StartCommit();
  return promise;
}

void DisplayLockContext::MakeResolver(ScriptState* script_state,
                                      Member<ScriptPromiseResolver>* resolver) {
  DCHECK(ConnectedToView());
  document_->View()->RegisterForLifecycleNotifications(this);
  *resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
}

bool DisplayLockContext::HasResolver() {
  return acquire_resolver_ || update_resolver_ || commit_resolver_;
}

ScriptPromise DisplayLockContext::updateAndCommit(ScriptState* script_state) {
  TRACE_EVENT0("blink", "DisplayLockContext::updateAndCommit()");

  // Resolve if we're already unlocked.
  if (state_ == kUnlocked)
    return GetResolvedPromise(script_state);

  // If we're in a state where a co-operative update doesn't make sense (e.g. we
  // haven't acquired the lock, or we're already sync committing), then do
  // whatever commit() would do.
  if (state_ == kCommitting || !ConnectedToView())
    return commit(script_state);

  // If we have a commit resolver already, return it.
  if (commit_resolver_) {
    // We must be in a second call to updateAndCommit(), meaning that we're in
    // the kUpdating state with a commit_resolver_.
    DCHECK_EQ(state_, kUpdating);
    return commit_resolver_->Promise();
  }

  CancelTimeoutTask();
  MakeResolver(script_state, &commit_resolver_);
  StartUpdateIfNeeded();
  return commit_resolver_->Promise();
}

void DisplayLockContext::FinishUpdateResolver(ResolverState state,
                                              const char* rejection_reason) {
  FinishResolver(&update_resolver_, state, rejection_reason);
}

void DisplayLockContext::FinishCommitResolver(ResolverState state,
                                              const char* rejection_reason) {
  FinishResolver(&commit_resolver_, state, rejection_reason);
}

void DisplayLockContext::FinishAcquireResolver(ResolverState state,
                                               const char* rejection_reason) {
  FinishResolver(&acquire_resolver_, state, rejection_reason);
}

void DisplayLockContext::FinishResolver(Member<ScriptPromiseResolver>* resolver,
                                        ResolverState state,
                                        const char* rejection_reason) {
  if (!*resolver)
    return;
  switch (state) {
    case kResolve:
      // In order to avoid script doing work as a part of the lifecycle update,
      // we delay the resolution to be in a task.
      GetExecutionContext()
          ->GetTaskRunner(TaskType::kMiscPlatformAPI)
          ->PostTask(FROM_HERE, WTF::Bind(
                                    +[](ScriptPromiseResolver* resolver) {
                                      resolver->Resolve();
                                    },
                                    WrapPersistent(resolver->Get())));
      break;
    case kReject:
      DCHECK(rejection_reason);
      (*resolver)->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError, rejection_reason));
      break;
    case kDetach:
      (*resolver)->Detach();
      break;
  }
  *resolver = nullptr;
  if (!HasResolver() && ConnectedToView())
    document_->View()->UnregisterFromLifecycleNotifications(this);
}

bool DisplayLockContext::ShouldPerformUpdatePhase(
    DisplayLockBudget::Phase phase) const {
  DCHECK(document_);
  if (state_ != kUpdating)
    return false;
  auto* view = document_->View();
  return view && view->InLifecycleUpdate() &&
         update_budget_->ShouldPerformPhase(phase,
                                            view->CurrentLifecycleData());
}

bool DisplayLockContext::ShouldStyle(LifecycleTarget target) const {
  return target == kSelf || update_forced_ || state_ > kUpdating ||
         ShouldPerformUpdatePhase(DisplayLockBudget::Phase::kStyle);
}

void DisplayLockContext::DidStyle(LifecycleTarget target) {
  if (state_ == kUnlocked)
    return;

  if (target == kSelf) {
    if (ForceUnlockIfNeeded())
      return;

    if (blocked_style_traversal_type_ == kStyleUpdateSelf)
      blocked_style_traversal_type_ = kStyleUpdateNotRequired;
    auto* layout_object = element_->GetLayoutObject();
    is_horizontal_writing_mode_ =
        !layout_object || layout_object->IsHorizontalWritingMode();
    return;
  }

  if (state_ != kCommitting && state_ != kUpdating && !update_forced_)
    return;

  blocked_style_traversal_type_ = kStyleUpdateNotRequired;

  MarkElementsForWhitespaceReattachment();

  if (state_ == kUpdating)
    update_budget_->DidPerformPhase(DisplayLockBudget::Phase::kStyle);
}

bool DisplayLockContext::ShouldLayout(LifecycleTarget target) const {
  return target == kSelf || update_forced_ || state_ > kUpdating ||
         ShouldPerformUpdatePhase(DisplayLockBudget::Phase::kLayout);
}

void DisplayLockContext::DidLayout(LifecycleTarget target) {
  if (target == kSelf)
    return;

  if (state_ == kUpdating)
    update_budget_->DidPerformPhase(DisplayLockBudget::Phase::kLayout);
}

bool DisplayLockContext::ShouldPrePaint() const {
  return update_forced_ || state_ > kUpdating ||
         ShouldPerformUpdatePhase(DisplayLockBudget::Phase::kPrePaint);
}

void DisplayLockContext::DidPrePaint() {
  if (state_ == kUpdating)
    update_budget_->DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);

#if DCHECK_IS_ON()
  if (state_ == kUpdating || state_ == kCommitting) {
    // Since we should be under containment, we should have a layer. If we
    // don't, then paint might not happen and we'll never resolve.
    DCHECK(element_->GetLayoutObject()->HasLayer());
  }
#endif
}

bool DisplayLockContext::ShouldPaint() const {
  // Note that forced updates should never require us to paint, so we don't
  // check |update_forced_| here. In other words, although |update_forced_|
  // could be true here, we still should not paint. This also holds for
  // kUpdating state, since updates should not paint.
  return state_ == kCommitting || state_ == kUnlocked;
}

void DisplayLockContext::DidPaint() {
  // This is here for symmetry, but could be removed if necessary.
}

bool DisplayLockContext::IsActivatable() const {
  return activatable_ || !IsLocked();
}

void DisplayLockContext::CommitForActivation() {
  DCHECK(element_);
  DCHECK(ConnectedToView());
  DCHECK(ShouldCommitForActivation());
  StartCommit();
}

bool DisplayLockContext::ShouldCommitForActivation() const {
  return IsActivatable() && state_ != kUnlocked && state_ != kCommitting;
}

void DisplayLockContext::DidAttachLayoutTree() {
  if (state_ >= kUnlocked)
    return;
  if (ForceUnlockIfNeeded())
    return;

  if (auto* layout_object = element_->GetLayoutObject())
    is_horizontal_writing_mode_ = layout_object->IsHorizontalWritingMode();
}

DisplayLockContext::ScopedForcedUpdate
DisplayLockContext::GetScopedForcedUpdate() {
  if (state_ >= kCommitting)
    return ScopedForcedUpdate(nullptr);

  DCHECK(!update_forced_);
  update_forced_ = true;

  // Now that the update is forced, we should ensure that style layout, and
  // prepaint code can reach it via dirty bits. Note that paint isn't a part of
  // this, since |update_forced_| doesn't force paint to happen. See
  // ShouldPaint().
  MarkForStyleRecalcIfNeeded();
  MarkAncestorsForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  return ScopedForcedUpdate(this);
}

void DisplayLockContext::NotifyForcedUpdateScopeEnded() {
  DCHECK(update_forced_);
  update_forced_ = false;
}

void DisplayLockContext::StartCommit() {
  // Since we are starting a commit, cancel the timeout task.
  CancelTimeoutTask();
  if (CleanupAndRejectCommitIfNotConnected())
    return;

  // If we have an acquire resolver, it means that we haven't had a chance to
  // run the lifecycle yet to clear the painted output. However, we're being
  // requested to commit. So, we can commit immediately and reject the acquire
  // callback at the same time.
  if (acquire_resolver_) {
    FinishAcquireResolver(kReject, rejection_names::kLockCommitted);
    FinishCommitResolver(kResolve);
    state_ = kUnlocked;
  } else if (state_ != kUpdating) {
    ScheduleAnimation();
  }

  // We might already be unlocked due to above, but we should still mark
  // ancestor chains for updates below.
  if (state_ < kCommitting)
    state_ = kCommitting;

  update_budget_.reset();

  // We're committing without a budget, so ensure we can reach style.
  MarkForStyleRecalcIfNeeded();

  auto* layout_object = element_->GetLayoutObject();
  // We might commit without connecting, so there is no layout object yet.
  if (!layout_object)
    return;

  // Now that we know we have a layout object, we should ensure that we can
  // reach the rest of the phases as well.
  MarkAncestorsForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  MarkPaintLayerNeedsRepaint();

  layout_object->SetNeedsLayoutAndPrefWidthsRecalc(
      layout_invalidation_reason::kDisplayLock);
}

void DisplayLockContext::StartUpdateIfNeeded() {
  // We should not be calling this if we're unlocked.
  DCHECK_NE(state_, kUnlocked);
  // Any state other than kLocked means that we are already in the process of
  // updating/committing, so we can piggy back on that process without kicking
  // off any new updates.
  if (state_ != kLocked)
    return;

  // We don't need to mark anything dirty since the budget will take care of
  // that for us.
  update_budget_ = CreateNewBudget();
  state_ = kUpdating;
  ScheduleAnimation();
}

std::unique_ptr<DisplayLockBudget> DisplayLockContext::CreateNewBudget() {
  switch (BudgetType::kDefault) {
    case BudgetType::kDoNotYield:
      return base::WrapUnique(new UnyieldingDisplayLockBudget(this));
    case BudgetType::kStrictYieldBetweenLifecyclePhases:
      return base::WrapUnique(new StrictYieldingDisplayLockBudget(this));
    case BudgetType::kYieldBetweenLifecyclePhases:
      return base::WrapUnique(new YieldingDisplayLockBudget(this));
  }
  NOTREACHED();
  return nullptr;
}

void DisplayLockContext::AddToWhitespaceReattachSet(Element& element) {
  whitespace_reattach_set_.insert(&element);
}

void DisplayLockContext::MarkElementsForWhitespaceReattachment() {
  for (auto element : whitespace_reattach_set_) {
    if (!element || element->NeedsReattachLayoutTree() ||
        !element->GetLayoutObject())
      continue;

    if (Node* first_child = LayoutTreeBuilderTraversal::FirstChild(*element))
      first_child->MarkAncestorsWithChildNeedsReattachLayoutTree();
  }
  whitespace_reattach_set_.clear();
}

bool DisplayLockContext::MarkForStyleRecalcIfNeeded() {
  if (IsElementDirtyForStyleRecalc()) {
    if (blocked_style_traversal_type_ > kStyleUpdateNotRequired) {
      // We blocked a traversal going to the element previously.
      // Make sure we will traverse this element and maybe its subtree if we
      // previously blocked a style traversal that should've done that.
      element_->SetNeedsStyleRecalc(
          blocked_style_traversal_type_ == kStyleUpdateDescendants
              ? kSubtreeStyleChange
              : kLocalStyleChange,
          StyleChangeReasonForTracing::Create(
              style_change_reason::kDisplayLock));
      if (blocked_style_traversal_type_ == kStyleUpdateChildren)
        element_->SetChildNeedsStyleRecalc();
      blocked_style_traversal_type_ = kStyleUpdateNotRequired;
    }
    // Propagate to the ancestors, since the dirty bit in a locked subtree is
    // stopped at the locked ancestor.
    // See comment in IsElementDirtyForStyleRecalc.
    element_->MarkAncestorsWithChildNeedsStyleRecalc();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkAncestorsForLayoutIfNeeded() {
  if (IsElementDirtyForLayout()) {
    element_->GetLayoutObject()->MarkContainerChainForLayout();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkAncestorsForPrePaintIfNeeded() {
  if (IsElementDirtyForPrePaint()) {
    auto* layout_object = element_->GetLayoutObject();
    if (auto* parent = layout_object->Parent())
      parent->SetSubtreeShouldCheckForPaintInvalidation();

    // Note that if either we or our descendants are marked as needing this
    // update, then ensure to mark self as needing the update. This sets up the
    // correct flags for PrePaint to recompute the necessary values and
    // propagate the information into the subtree.
    if (needs_effective_allowed_touch_action_update_ ||
        layout_object->EffectiveAllowedTouchActionChanged() ||
        layout_object->DescendantEffectiveAllowedTouchActionChanged()) {
      layout_object->MarkEffectiveAllowedTouchActionChanged();
    }
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkPaintLayerNeedsRepaint() {
  DCHECK(ConnectedToView());
  if (auto* layout_object = element_->GetLayoutObject()) {
    layout_object->PaintingLayer()->SetNeedsRepaint();
    if (needs_graphics_layer_collection_) {
      document_->View()->GraphicsLayersDidChange();
      needs_graphics_layer_collection_ = false;
    }
    return true;
  }
  return false;
}

bool DisplayLockContext::IsElementDirtyForStyleRecalc() const {
  // The |element_| checks could be true even if |blocked_style_traversal_type_|
  // is not required. The reason for this is that the
  // blocked_style_traversal_type_ is set during the style walk that this
  // display lock blocked. However, we could dirty element style and commit
  // before ever having gone through the style calc that would have been
  // blocked, meaning we never blocked style during a walk. Instead we might
  // have not propagated the dirty bits up the tree.
  return element_->NeedsStyleRecalc() || element_->ChildNeedsStyleRecalc() ||
         blocked_style_traversal_type_ > kStyleUpdateNotRequired;
}

bool DisplayLockContext::IsElementDirtyForLayout() const {
  if (auto* layout_object = element_->GetLayoutObject())
    return layout_object->NeedsLayout();
  return false;
}

bool DisplayLockContext::IsElementDirtyForPrePaint() const {
  if (auto* layout_object = element_->GetLayoutObject()) {
    return PrePaintTreeWalk::ObjectRequiresPrePaint(*layout_object) ||
           PrePaintTreeWalk::ObjectRequiresTreeBuilderContext(*layout_object) ||
           needs_prepaint_subtree_walk_ ||
           needs_effective_allowed_touch_action_update_;
  }
  return false;
}

void DisplayLockContext::DidMoveToNewDocument(Document& old_document) {
  DCHECK(element_);
  document_ = &element_->GetDocument();

  // Since we're observing the lifecycle updates, ensure that we listen to the
  // right document's view.
  if (HasResolver()) {
    if (old_document.View())
      old_document.View()->UnregisterFromLifecycleNotifications(this);
    if (document_->View())
      document_->View()->RegisterForLifecycleNotifications(this);
  }

  if (!IsActivatable()) {
    old_document.RemoveActivationBlockingDisplayLock();
    document_->AddActivationBlockingDisplayLock();
  }
  if (IsLocked()) {
    old_document.RemoveLockedDisplayLock();
    document_->AddLockedDisplayLock();
  }
}

void DisplayLockContext::WillStartLifecycleUpdate(const LocalFrameView& view) {
  if (update_budget_)
    update_budget_->OnLifecycleChange(view.CurrentLifecycleData());
}

void DisplayLockContext::DidFinishLifecycleUpdate(const LocalFrameView& view) {
  if (acquire_resolver_) {
    if (ForceUnlockIfNeeded())
      return;
    FinishAcquireResolver(kResolve);
    // TODO(vmpstr): When size: auto is supported, we need to get the size from
    // the layout object here.
    // Fallthrough here in case we're already updating.
  }

  if (state_ == kCommitting) {
    FinishUpdateResolver(kResolve);
    FinishCommitResolver(kResolve);
    CancelTimeoutTask();
    state_ = kUnlocked;
    return;
  }

  if (state_ != kUpdating)
    return;

  // If we became disconnected for any reason, then we should reject the
  // update promise and go back to the locked state.
  if (!element_ || !ConnectedToView()) {
    FinishUpdateResolver(kReject, rejection_names::kElementIsDisconnected);
    update_budget_.reset();

    if (commit_resolver_) {
      // We resolve the commit if we're not connected.
      FinishCommitResolver(kResolve);
      CancelTimeoutTask();
      state_ = kUnlocked;
    } else {
      state_ = kLocked;
    }
    return;
  }

  if (update_budget_->NeedsLifecycleUpdates()) {
    // Note that we post a task to schedule an animation, since rAF requests can
    // be ignored if they happen from within a lifecycle update.
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE, WTF::Bind(&DisplayLockContext::ScheduleAnimation,
                                        WrapWeakPersistent(this)));
    return;
  }

  FinishUpdateResolver(kResolve);
  update_budget_.reset();
  state_ = kLocked;

  if (commit_resolver_) {
    // Schedule a commit to run. Note that we can't call StartCommit directly
    // here, since we're in the lifecycle updates right now and the code that
    // runs after may depend on having clean layout state, which StartCommit
    // might dirty.
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE, WTF::Bind(&DisplayLockContext::StartCommit,
                                        WrapWeakPersistent(this)));
  }
}

void DisplayLockContext::NotifyWillDisconnect() {
  if (!IsLocked() || !element_ || !element_->GetLayoutObject())
    return;
  // If we're locked while being disconnected, we need to layout the parent.
  // The reason for this is that we might skip the layout if we're empty while
  // locked, but it's important to update IsSelfCollapsingBlock property on
  // the parent so that it's up to date. This property is updated during
  // layout.
  if (auto* parent = element_->GetLayoutObject()->Parent())
    parent->SetNeedsLayout(layout_invalidation_reason::kDisplayLock);
}

void DisplayLockContext::ScheduleAnimation() {
  DCHECK(element_);
  DCHECK(ConnectedToView());

  // Schedule an animation to perform the lifecycle phases.
  document_->GetPage()->Animator().ScheduleVisualUpdate(document_->GetFrame());
}

void DisplayLockContext::RescheduleTimeoutTask(double delay) {
  CancelTimeoutTask();

  if (!std::isfinite(delay))
    return;

  // Make sure the delay is at least 1ms.
  delay = std::max(delay, 1.);
  timeout_task_handle_ = PostDelayedCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI),
      FROM_HERE,
      WTF::Bind(&DisplayLockContext::TriggerTimeout, WrapWeakPersistent(this)),
      TimeDelta::FromMillisecondsD(delay));
}

void DisplayLockContext::CancelTimeoutTask() {
  timeout_task_handle_.Cancel();
}

void DisplayLockContext::TriggerTimeout() {
  // We might have started destroyed the element or started to shut down while
  // we're triggering a timeout. In that case, do nothing.
  if (!element_ || !document_->Lifecycle().IsActive())
    return;
  StartCommit();
}

const char* DisplayLockContext::ShouldForceUnlock() const {
  DCHECK(element_);
  // The style can be dirty if we're in a nested lock.
  // TODO(vmpstr): We need to figure out what to do here, since we don't know
  // what the style is and whether this element has proper containment. However,
  // forcing an update from the ancestor locks seems inefficient. For now, we
  // just optimistically assume that we have all of the right containment in
  // place. See crbug.com/926276 for more information.
  if (element_->NeedsStyleRecalc())
    return nullptr;

  if (element_->HasDisplayContentsStyle())
    return rejection_names::kUnsupportedDisplay;

  // If we have a layout object, check that since it's a more authoritative
  // source of containment information.
  if (auto* layout_object = element_->GetLayoutObject()) {
    if (layout_object->ShouldApplyStyleContainment() &&
        layout_object->ShouldApplyLayoutContainment())
      return nullptr;
    return rejection_names::kContainmentNotSatisfied;
  }

  // Otherwise, fallback on just checking style.
  auto* style = element_->GetComputedStyle();
  // Note that if for whatever reason we don't have computed style, then
  // optimistically assume that we have containment.
  // TODO(vmpstr): Perhaps we need to add render=lockable which will ensure
  // containment.
  if (!style || (style->ContainsStyle() && style->ContainsLayout()))
    return nullptr;
  return rejection_names::kContainmentNotSatisfied;
}

bool DisplayLockContext::ForceUnlockIfNeeded() {
  // We must have "contain: style layout", and disallow display:contents
  // for display locking. Note that we should always guarantee this after
  // every style or layout tree update. Otherwise, proceeding with layout may
  // cause unexpected behavior. By rejecting the promise, the behavior can be
  // detected by script.
  // TODO(rakina): If this is after acquire's promise is resolved and update()
  // commit() isn't in progress, the web author won't know that the element
  // got unlocked. Figure out how to notify the author.
  if (auto* reason = ShouldForceUnlock()) {
    FinishAcquireResolver(kReject, reason);
    FinishUpdateResolver(kReject, reason);
    FinishCommitResolver(kReject, reason);
    CancelTimeoutTask();
    state_ = kUnlocked;
    return true;
  }
  return false;
}

bool DisplayLockContext::ConnectedToView() const {
  DCHECK(element_);
  return element_->isConnected() && document_->View();
}

// Scoped objects implementation
// -----------------------------------------------

DisplayLockContext::ScopedForcedUpdate::ScopedForcedUpdate(
    DisplayLockContext* context)
    : context_(context) {}

DisplayLockContext::ScopedForcedUpdate::ScopedForcedUpdate(
    ScopedForcedUpdate&& other)
    : context_(other.context_) {
  other.context_ = nullptr;
}

DisplayLockContext::ScopedForcedUpdate::~ScopedForcedUpdate() {
  if (context_)
    context_->NotifyForcedUpdateScopeEnded();
}

// StateChangeHelper implementation
// -----------------------------------------------
DisplayLockContext::StateChangeHelper::StateChangeHelper(
    DisplayLockContext* context)
    : context_(context) {}

DisplayLockContext::StateChangeHelper& DisplayLockContext::StateChangeHelper::
operator=(State new_state) {
  if (new_state == state_)
    return *this;

  if (state_ == kUnlocked) {
    TRACE_EVENT_ASYNC_BEGIN0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
        "LockedDisplayLock", this);
  } else if (new_state == kUnlocked) {
    TRACE_EVENT_ASYNC_END0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
        "LockedDisplayLock", this);
  }

  bool was_activatable = context_->IsActivatable();
  bool was_locked = context_->IsLocked();

  state_ = new_state;
  if (state_ != kUnlocked) {
    TRACE_EVENT_ASYNC_STEP_INTO0(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
        "LockedDisplayLock", this, StateToString(state_));
  }

  if (!context_->document_)
    return *this;

  UpdateActivationBlockingCount(was_activatable, context_->IsActivatable());

  // Adjust the total number of locked display locks.
  auto& document = *context_->document_;
  if (context_->IsLocked() != was_locked) {
    if (was_locked)
      document.RemoveLockedDisplayLock();
    else
      document.AddLockedDisplayLock();
  }

  return *this;
}

void DisplayLockContext::StateChangeHelper::UpdateActivationBlockingCount(
    bool old_activatable,
    bool new_activatable) {
  auto& document = *context_->document_;
  // Adjust activation blocking lock counts.
  if (old_activatable != new_activatable) {
    if (old_activatable)
      document.AddActivationBlockingDisplayLock();
    else
      document.RemoveActivationBlockingDisplayLock();
  }
}

}  // namespace blink
