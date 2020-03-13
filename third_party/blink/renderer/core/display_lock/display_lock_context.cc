// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_recalc.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/display_lock/render_subtree_activation_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
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
namespace rejection_names {
const char* kContainmentNotSatisfied =
    "Containment requirement is not satisfied.";
const char* kUnsupportedDisplay =
    "Element has unsupported display type (display: contents).";
}  // namespace rejection_names

void RecordActivationReason(DisplayLockActivationReason reason) {
  int ordered_reason = -1;

  // IMPORTANT: This number needs to be bumped up when adding
  // new reasons.
  static const int number_of_reasons = 9;

  switch (reason) {
    case DisplayLockActivationReason::kAccessibility:
      ordered_reason = 0;
      break;
    case DisplayLockActivationReason::kFindInPage:
      ordered_reason = 1;
      break;
    case DisplayLockActivationReason::kFragmentNavigation:
      ordered_reason = 2;
      break;
    case DisplayLockActivationReason::kScriptFocus:
      ordered_reason = 3;
      break;
    case DisplayLockActivationReason::kScrollIntoView:
      ordered_reason = 4;
      break;
    case DisplayLockActivationReason::kSelection:
      ordered_reason = 5;
      break;
    case DisplayLockActivationReason::kSimulatedClick:
      ordered_reason = 6;
      break;
    case DisplayLockActivationReason::kUserFocus:
      ordered_reason = 7;
      break;
    case DisplayLockActivationReason::kViewportIntersection:
      ordered_reason = 8;
      break;
    case DisplayLockActivationReason::kViewport:
    case DisplayLockActivationReason::kAny:
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Blink.Render.DisplayLockActivationReason",
                            ordered_reason, number_of_reasons);
}
}  // namespace

DisplayLockContext::DisplayLockContext(Element* element)
    : element_(element), document_(&element_->GetDocument()) {
  document_->AddDisplayLockContext(this);
}

void DisplayLockContext::SetRequestedState(ESubtreeVisibility state) {
  if (state_ == state)
    return;
  state_ = state;
  switch (state_) {
    case ESubtreeVisibility::kVisible:
      RequestUnlock();
      break;
    case ESubtreeVisibility::kAuto:
      RequestLock(static_cast<uint16_t>(DisplayLockActivationReason::kAny));
      break;
    case ESubtreeVisibility::kHidden:
      RequestLock(0u);
      break;
    case ESubtreeVisibility::kHiddenMatchable:
      RequestLock(
          static_cast<uint16_t>(DisplayLockActivationReason::kAny) &
          ~static_cast<uint16_t>(DisplayLockActivationReason::kViewport));
      break;
  }
}

void DisplayLockContext::AdjustElementStyle(ComputedStyle* style) const {
  if (state_ == ESubtreeVisibility::kVisible)
    return;
  // If not visible, element gains style and layout containment. If skipped, it
  // also gains size containment.
  // https://wicg.github.io/display-locking/#subtree-visibility
  auto contain = style->Contain() | kContainsStyle | kContainsLayout;
  if (IsLocked())
    contain |= kContainsSize;
  style->SetContain(contain);
}

void DisplayLockContext::Trace(Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(document_);
  visitor->Trace(whitespace_reattach_set_);
}

void DisplayLockContext::UpdateActivationObservationIfNeeded() {
  if (!document_) {
    is_observed_ = false;
    is_registered_for_lifecycle_notifications_ = false;
    needs_intersection_lock_check_ = false;
    return;
  }

  // We require observation if we are viewport-activatable, and one of the
  // following is true:
  // 1. We're locked, which means that we need to know when to unlock the
  //    element
  // 2. We're activated (in the CSS version), which means that we need to know
  //    when we stop intersecting the viewport so that we can re-lock.
  bool should_observe =
      lock_requested_ &&
      IsActivatable(DisplayLockActivationReason::kViewportIntersection) &&
      ConnectedToView();
  if (should_observe && !is_observed_) {
    document_->RegisterDisplayLockActivationObservation(element_);
  } else if (!should_observe && is_observed_) {
    document_->UnregisterDisplayLockActivationObservation(element_);
    // We don't need intersection lock checks if we are not observing
    // intersections anymore.
    needs_intersection_lock_check_ = false;
    UpdateLifecycleNotificationRegistration();
  }
  is_observed_ = should_observe;
}

bool DisplayLockContext::NeedsLifecycleNotifications() const {
  return needs_intersection_lock_check_;
}

void DisplayLockContext::UpdateLifecycleNotificationRegistration() {
  if (!document_ || !document_->View()) {
    is_registered_for_lifecycle_notifications_ = false;
    return;
  }

  bool needs_notifications = NeedsLifecycleNotifications();
  if (needs_notifications == is_registered_for_lifecycle_notifications_)
    return;

  is_registered_for_lifecycle_notifications_ = needs_notifications;
  if (needs_notifications) {
    document_->View()->RegisterForLifecycleNotifications(this);
  } else {
    document_->View()->UnregisterFromLifecycleNotifications(this);
  }
}

void DisplayLockContext::UpdateActivationBlockingCount(bool was_activatable,
                                                       bool is_activatable) {
  DCHECK(document_);
  if (was_activatable != is_activatable) {
    if (was_activatable)
      document_->IncrementDisplayLockBlockingAllActivation();
    else
      document_->DecrementDisplayLockBlockingAllActivation();
  }
}

void DisplayLockContext::SetActivatable(uint16_t activatable_mask) {
  if (activatable_mask == activatable_mask_)
    return;
  // If we're locked, the activatable mask might change the activation
  // blocking lock count. If we're not locked, the activation blocking lock
  // count will be updated when we lock.
  // Note that we record this only if we're blocking all activation. That is,
  // the lock is considered activatable if any bit is set.
  if (IsLocked())
    UpdateActivationBlockingCount(activatable_mask_, activatable_mask);

  activatable_mask_ = activatable_mask;
  UpdateActivationObservationIfNeeded();

  ClearActivated();
}

void DisplayLockContext::StartAcquire() {
  DCHECK(lock_requested_);
  DCHECK(!IsLocked());

  is_locked_ = true;
  document_->AddLockedDisplayLock();
  if (!activatable_mask_)
    document_->IncrementDisplayLockBlockingAllActivation();
  UpdateActivationObservationIfNeeded();

  needs_intersection_lock_check_ = false;
  UpdateLifecycleNotificationRegistration();

  if (RuntimeEnabledFeatures::CSSSubtreeVisibilityActivationEventEnabled()) {
    // We're no longer activated, so if the signal didn't run yet, we should
    // cancel it.
    weak_factory_.InvalidateWeakPtrs();
  }

  // If we're already connected then we need to ensure that we update our style
  // to check for containment later, layout size based on the options, and
  // also clear the painted output.
  if (!ConnectedToView())
    return;

  // There are several ways we can call StartAcquire. Most of them require us to
  // dirty style so that we can add proper containment onto the element.
  // However, if we're doing a StartAcquire from within style recalc, then we
  // don't need to do anything as we should have already added containment.
  // Moreover, dirtying self style from within style recalc is not allowed,
  // since either it has no effect and is cleaned before any work is done, or it
  // causes DCHECKs in AssertLayoutTreeUpdated().
  if (!document_->InStyleRecalc()) {
    element_->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kDisplayLock));
  }

  // In either case, we schedule an animation. If we're already inside a
  // lifecycle update, this will be a no-op.
  ScheduleAnimation();

  // We need to notify the AX cache (if it exists) to update |element_|'s
  // children in the AX cache.
  if (AXObjectCache* cache = element_->GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(element_);

  auto* layout_object = element_->GetLayoutObject();
  if (!layout_object) {
    is_horizontal_writing_mode_ = true;
    return;
  }

  layout_object->SetNeedsLayoutAndPrefWidthsRecalc(
      layout_invalidation_reason::kDisplayLock);

  is_horizontal_writing_mode_ = layout_object->IsHorizontalWritingMode();

  // GraphicsLayer collection would normally skip layers if paint is blocked
  // by display-locking (see: CollectDrawableLayersForLayerListRecursively
  // in LocalFrameView). However, if we don't trigger this collection, then
  // we might use the cached result instead. In order to ensure we skip the
  // newly locked layers, we need to set |need_graphics_layer_collection_|
  // before marking the layer for repaint.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    needs_graphics_layer_collection_ = true;
  MarkPaintLayerNeedsRepaint();
}

bool DisplayLockContext::ShouldStyle(DisplayLockLifecycleTarget target) const {
  return !is_locked_ || target == DisplayLockLifecycleTarget::kSelf ||
         update_forced_ ||
         (document_->ActivatableDisplayLocksForced() &&
          IsActivatable(DisplayLockActivationReason::kAny));
}

void DisplayLockContext::DidStyle(DisplayLockLifecycleTarget target) {
  if (target == DisplayLockLifecycleTarget::kSelf) {
    if (ForceUnlockIfNeeded())
      return;

    if (blocked_style_traversal_type_ == kStyleUpdateSelf)
      blocked_style_traversal_type_ = kStyleUpdateNotRequired;
    auto* layout_object = element_->GetLayoutObject();
    is_horizontal_writing_mode_ =
        !layout_object || layout_object->IsHorizontalWritingMode();
    return;
  }

  if (element_->ChildNeedsReattachLayoutTree())
    element_->MarkAncestorsWithChildNeedsReattachLayoutTree();
  blocked_style_traversal_type_ = kStyleUpdateNotRequired;
  MarkElementsForWhitespaceReattachment();
}

bool DisplayLockContext::ShouldLayout(DisplayLockLifecycleTarget target) const {
  return !is_locked_ || target == DisplayLockLifecycleTarget::kSelf ||
         update_forced_ ||
         (document_->ActivatableDisplayLocksForced() &&
          IsActivatable(DisplayLockActivationReason::kAny));
}

void DisplayLockContext::DidLayout(DisplayLockLifecycleTarget target) {
  if (target == DisplayLockLifecycleTarget::kSelf)
    return;

  // Since we did layout on children already, we'll clear this.
  child_layout_was_blocked_ = false;
}

bool DisplayLockContext::ShouldPrePaint(
    DisplayLockLifecycleTarget target) const {
  return !is_locked_ || target == DisplayLockLifecycleTarget::kSelf ||
         update_forced_;
}

void DisplayLockContext::DidPrePaint(DisplayLockLifecycleTarget target) {
  // This is here for symmetry, but could be removed if necessary.
}

bool DisplayLockContext::ShouldPaint(DisplayLockLifecycleTarget target) const {
  // Note that forced updates should never require us to paint, so we don't
  // check |update_forced_| here. In other words, although |update_forced_|
  // could be true here, we still should not paint. This also holds for
  // kUpdating state, since updates should not paint.
  return !is_locked_ || target == DisplayLockLifecycleTarget::kSelf;
}

void DisplayLockContext::DidPaint(DisplayLockLifecycleTarget) {
  // This is here for symmetry, but could be removed if necessary.
}

bool DisplayLockContext::IsActivatable(
    DisplayLockActivationReason reason) const {
  return activatable_mask_ & static_cast<uint16_t>(reason);
}

void DisplayLockContext::FireActivationEvent(Element* activated_element) {
  element_->DispatchEvent(
      *MakeGarbageCollected<RenderSubtreeActivationEvent>(*activated_element));
}

void DisplayLockContext::CommitForActivationWithSignal(
    Element* activated_element,
    DisplayLockActivationReason reason_for_metrics) {
  DCHECK(activated_element);
  DCHECK(element_);
  DCHECK(ConnectedToView());
  DCHECK(IsLocked());
  DCHECK(ShouldCommitForActivation(DisplayLockActivationReason::kAny));

  // TODO(vmpstr): Remove this when we have a beforematch event.
  if (RuntimeEnabledFeatures::CSSSubtreeVisibilityActivationEventEnabled()) {
    document_->EnqueueDisplayLockActivationTask(
        WTF::Bind(&DisplayLockContext::FireActivationEvent,
                  weak_factory_.GetWeakPtr(), WrapPersistent(activated_element)));
  }

  StartCommit();

  RecordActivationReason(reason_for_metrics);
  if (reason_for_metrics == DisplayLockActivationReason::kFindInPage)
    document_->MarkHasFindInPageSubtreeVisibilityActiveMatch();

  is_activated_ = true;
  // Since size containment depends on the activatability state, we should
  // invalidate the style for this element, so that the style adjuster can
  // properly remove the containment.
  element_->SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kDisplayLock));
}

bool DisplayLockContext::IsActivated() const {
  return is_activated_;
}

void DisplayLockContext::ClearActivated() {
  is_activated_ = false;
  // If we are no longer activated, then we're either committing or acquiring a
  // lock. In either case, we don't need to rely on lifecycle observations to
  // become hidden.
  // TODO(vmpstr): This needs refactoring.
  needs_intersection_lock_check_ = false;
  UpdateLifecycleNotificationRegistration();
}

void DisplayLockContext::NotifyIsIntersectingViewport() {
  // If we are now intersecting, then we are definitely not nested in a locked
  // subtree and we don't need to lock as a result.
  needs_intersection_lock_check_ = false;
  UpdateLifecycleNotificationRegistration();

  if (!IsLocked())
    return;

  DCHECK(IsActivatable(DisplayLockActivationReason::kViewportIntersection));
  CommitForActivationWithSignal(
      element_, DisplayLockActivationReason::kViewportIntersection);
}

void DisplayLockContext::NotifyIsNotIntersectingViewport() {
  if (IsLocked()) {
    DCHECK(!needs_intersection_lock_check_);
    return;
  }

  // There are two situations we need to consider here:
  // 1. We are off-screen but not nested in any other lock. This means we should
  //    re-lock (also verify that the reason we're in this state is that we're
  //    activated).
  // 2. We are in a nested locked context. This means we don't actually know
  //    whether we should lock or not. In order to avoid needless dirty of the
  //    layout and style trees up to the nested context, we remain unlocked.
  //    However, we also need to ensure that we relock if we become unnested.
  //    So, we simply delay this check to the next frame (via LocalFrameView),
  //    which will call this function again and so we can perform the check
  //    again.
  DCHECK(ConnectedToView());
  auto* locked_ancestor =
      DisplayLockUtilities::NearestLockedExclusiveAncestor(*element_);
  if (locked_ancestor) {
    needs_intersection_lock_check_ = true;
    UpdateLifecycleNotificationRegistration();
  } else {
    DCHECK(IsActivated());
    ClearActivated();
    StartAcquire();
    DCHECK(!needs_intersection_lock_check_);
  }
}

bool DisplayLockContext::RequestLock(uint16_t activation_mask) {
  SetActivatable(activation_mask);

  if (IsLocked()) {
    DCHECK(lock_requested_);
    return true;
  }
  lock_requested_ = true;

  if (IsActivated())
    return false;

  StartAcquire();
  return true;
}

void DisplayLockContext::RequestUnlock() {
  lock_requested_ = false;
  ClearActivated();
  if (IsLocked())
    StartCommit();
}

bool DisplayLockContext::ShouldCommitForActivation(
    DisplayLockActivationReason reason) const {
  return IsActivatable(reason) && IsLocked();
}

void DisplayLockContext::DidAttachLayoutTree() {
  if (auto* layout_object = element_->GetLayoutObject())
    is_horizontal_writing_mode_ = layout_object->IsHorizontalWritingMode();
}

DisplayLockContext::ScopedForcedUpdate
DisplayLockContext::GetScopedForcedUpdate() {
  if (!is_locked_)
    return ScopedForcedUpdate(nullptr);

  DCHECK(!update_forced_);
  update_forced_ = true;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"), "LockForced",
      TRACE_ID_LOCAL(this));

  // Now that the update is forced, we should ensure that style layout, and
  // prepaint code can reach it via dirty bits. Note that paint isn't a part of
  // this, since |update_forced_| doesn't force paint to happen. See
  // ShouldPaint().
  MarkForStyleRecalcIfNeeded();
  MarkForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  return ScopedForcedUpdate(this);
}

void DisplayLockContext::NotifyForcedUpdateScopeEnded() {
  DCHECK(update_forced_);
  update_forced_ = false;
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"), "LockForced",
      TRACE_ID_LOCAL(this));
}

void DisplayLockContext::StartCommit() {
  DCHECK(IsLocked());
  if (!ConnectedToView()) {
    is_locked_ = false;
    UpdateActivationObservationIfNeeded();
    return;
  }

  ScheduleAnimation();
  is_locked_ = false;
  document_->RemoveLockedDisplayLock();
  if (!activatable_mask_)
    document_->DecrementDisplayLockBlockingAllActivation();
  UpdateActivationObservationIfNeeded();

  // We skip updating the style dirtiness if we're within style recalc. This is
  // instead handled by a call to AdjustStyleRecalcChangeForChildren().
  if (!document_->InStyleRecalc())
    MarkForStyleRecalcIfNeeded();

  // We also need to notify the AX cache (if it exists) to update the childrens
  // of |element_| in the AX cache.
  if (AXObjectCache* cache = element_->GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(element_);

  auto* layout_object = element_->GetLayoutObject();
  // We might commit without connecting, so there is no layout object yet.
  if (!layout_object)
    return;

  // Now that we know we have a layout object, we should ensure that we can
  // reach the rest of the phases as well.
  MarkForLayoutIfNeeded();
  MarkAncestorsForPrePaintIfNeeded();
  MarkPaintLayerNeedsRepaint();

  layout_object->SetNeedsLayoutAndPrefWidthsRecalc(
      layout_invalidation_reason::kDisplayLock);
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

StyleRecalcChange DisplayLockContext::AdjustStyleRecalcChangeForChildren(
    StyleRecalcChange change) {
  // This code is similar to MarkForStyleRecalcIfNeeded, except that it acts on
  // |change| and not on |element_|. This is only called during style recalc.
  // Note that since we're already in self style recalc, this code is shorter
  // since it doesn't have to deal with dirtying self-style.
  DCHECK(document_->InStyleRecalc());

  if (reattach_layout_tree_was_blocked_) {
    change = change.ForceReattachLayoutTree();
    reattach_layout_tree_was_blocked_ = false;
  }

  if (blocked_style_traversal_type_ == kStyleUpdateDescendants)
    change = change.ForceRecalcDescendants();
  else if (blocked_style_traversal_type_ == kStyleUpdateChildren)
    change = change.EnsureAtLeast(StyleRecalcChange::kRecalcChildren);
  blocked_style_traversal_type_ = kStyleUpdateNotRequired;
  return change;
}

bool DisplayLockContext::MarkForStyleRecalcIfNeeded() {
  if (reattach_layout_tree_was_blocked_) {
    // We previously blocked a layout tree reattachment on |element_|'s
    // descendants, so we should mark it for layout tree reattachment now.
    element_->SetForceReattachLayoutTree();
    reattach_layout_tree_was_blocked_ = false;
  }
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
    } else if (element_->ChildNeedsReattachLayoutTree()) {
      // Mark |element_| as style dirty, as we can't mark for child reattachment
      // before style.
      element_->SetNeedsStyleRecalc(kLocalStyleChange,
                                    StyleChangeReasonForTracing::Create(
                                        style_change_reason::kDisplayLock));
    }
    // Propagate to the ancestors, since the dirty bit in a locked subtree is
    // stopped at the locked ancestor.
    // See comment in IsElementDirtyForStyleRecalc.
    element_->MarkAncestorsWithChildNeedsStyleRecalc();
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkForLayoutIfNeeded() {
  if (IsElementDirtyForLayout()) {
    // Forces the marking of ancestors to happen, even if
    // |DisplayLockContext::ShouldLayout()| returns false.
    base::AutoReset<bool> scoped_force(&update_forced_, true);
    if (child_layout_was_blocked_) {
      // We've previously blocked a child traversal when doing self-layout for
      // the locked element, so we're marking it with child-needs-layout so that
      // it will traverse to the locked element and do the child traversal
      // again. We don't need to mark it for self-layout (by calling
      // |LayoutObject::SetNeedsLayout()|) because the locked element itself
      // doesn't need to relayout.
      element_->GetLayoutObject()->SetChildNeedsLayout();
      child_layout_was_blocked_ = false;
    } else {
      // Since the dirty layout propagation stops at the locked element, we need
      // to mark its ancestors as dirty here so that it will be traversed to on
      // the next layout.
      element_->GetLayoutObject()->MarkContainerChainForLayout();
    }
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkAncestorsForPrePaintIfNeeded() {
  // TODO(vmpstr): We should add a compositing phase for proper bookkeeping.
  bool compositing_dirtied = MarkForCompositingUpdatesIfNeeded();

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
      // Note that although the object itself should have up to date value, in
      // order to force recalc of the whole subtree, we mark it as needing an
      // update.
      layout_object->MarkEffectiveAllowedTouchActionChanged();
    }
    return true;
  }
  return compositing_dirtied;
}

bool DisplayLockContext::MarkPaintLayerNeedsRepaint() {
  DCHECK(ConnectedToView());
  if (auto* layout_object = element_->GetLayoutObject()) {
    layout_object->PaintingLayer()->SetNeedsRepaint();
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        needs_graphics_layer_collection_) {
      document_->View()->SetForeignLayerListNeedsUpdate();
      needs_graphics_layer_collection_ = false;
    }
    return true;
  }
  return false;
}

bool DisplayLockContext::MarkForCompositingUpdatesIfNeeded() {
  if (!ConnectedToView())
    return false;

  auto* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return false;

  auto* layout_box = DynamicTo<LayoutBoxModelObject>(layout_object);
  if (layout_box && layout_box->HasSelfPaintingLayer()) {
    if (layout_box->Layer()->ChildNeedsCompositingInputsUpdate() &&
        layout_box->Layer()->Parent()) {
      // Note that if the layer's child needs compositing inputs update, then
      // that layer itself also needs compositing inputs update. In order to
      // propagate the dirty bit, we need to mark this layer's _parent_ as a
      // needing an update.
      layout_box->Layer()->Parent()->SetNeedsCompositingInputsUpdate();
    }
    if (needs_compositing_requirements_update_)
      layout_box->Layer()->SetNeedsCompositingRequirementsUpdate();
    needs_compositing_requirements_update_ = false;
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
         element_->ChildNeedsReattachLayoutTree() ||
         blocked_style_traversal_type_ > kStyleUpdateNotRequired;
}

bool DisplayLockContext::IsElementDirtyForLayout() const {
  if (auto* layout_object = element_->GetLayoutObject())
    return layout_object->NeedsLayout() || child_layout_was_blocked_;
  return false;
}

bool DisplayLockContext::IsElementDirtyForPrePaint() const {
  if (auto* layout_object = element_->GetLayoutObject()) {
    auto* layout_box = DynamicTo<LayoutBoxModelObject>(layout_object);
    return PrePaintTreeWalk::ObjectRequiresPrePaint(*layout_object) ||
           PrePaintTreeWalk::ObjectRequiresTreeBuilderContext(*layout_object) ||
           needs_prepaint_subtree_walk_ ||
           needs_effective_allowed_touch_action_update_ ||
           needs_compositing_requirements_update_ ||
           (layout_box && layout_box->HasSelfPaintingLayer() &&
            layout_box->Layer()->ChildNeedsCompositingInputsUpdate());
  }
  return false;
}

void DisplayLockContext::DidMoveToNewDocument(Document& old_document) {
  DCHECK(element_);
  document_ = &element_->GetDocument();

  old_document.RemoveDisplayLockContext(this);
  document_->AddDisplayLockContext(this);

  if (is_observed_) {
    old_document.UnregisterDisplayLockActivationObservation(element_);
    document_->RegisterDisplayLockActivationObservation(element_);
  }

  // Since we're observing the lifecycle updates, ensure that we listen to the
  // right document's view.
  if (is_registered_for_lifecycle_notifications_) {
    if (old_document.View())
      old_document.View()->UnregisterFromLifecycleNotifications(this);

    if (document_->View())
      document_->View()->RegisterForLifecycleNotifications(this);
    else
      is_registered_for_lifecycle_notifications_ = false;
  }

  if (IsLocked()) {
    old_document.RemoveLockedDisplayLock();
    document_->AddLockedDisplayLock();
    if (!IsActivatable(DisplayLockActivationReason::kAny)) {
      old_document.DecrementDisplayLockBlockingAllActivation();
      document_->IncrementDisplayLockBlockingAllActivation();
    }
  }
}

void DisplayLockContext::WillStartLifecycleUpdate(const LocalFrameView& view) {
  DCHECK(NeedsLifecycleNotifications());
  // We might have delayed processing intersection observation update (signal
  // that we were not intersecting) because this context was nested in another
  // locked context. At the start of the lifecycle, we should check whether
  // that is still true. In other words, this call will check if we're still
  // nested. If we are, we won't do anything. If we're not, then we will lock
  // this context.
  //
  // Note that when we are no longer nested and and we have not received any
  // notifications from the intersection observer, it means that we are not
  // visible.
  if (needs_intersection_lock_check_)
    NotifyIsNotIntersectingViewport();
}

void DisplayLockContext::DidFinishLifecycleUpdate(const LocalFrameView& view) {
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

void DisplayLockContext::ElementDisconnected() {
  UpdateActivationObservationIfNeeded();
}

void DisplayLockContext::ElementConnected() {
  UpdateActivationObservationIfNeeded();
}

void DisplayLockContext::ScheduleAnimation() {
  DCHECK(element_);
  if (!ConnectedToView() || !document_ || !document_->GetPage())
    return;

  // Schedule an animation to perform the lifecycle phases.
  document_->GetPage()->Animator().ScheduleVisualUpdate(document_->GetFrame());
}

const char* DisplayLockContext::ShouldForceUnlock() const {
  DCHECK(element_);
  // This function is only called after style, layout tree, or lifecycle
  // updates, so the style should be up-to-date, except in the case of nested
  // locks, where the style recalc will never actually get to |element_|.
  // TODO(vmpstr): We need to figure out what to do here, since we don't know
  // what the style is and whether this element has proper containment. However,
  // forcing an update from the ancestor locks seems inefficient. For now, we
  // just optimistically assume that we have all of the right containment in
  // place. See crbug.com/926276 for more information.
  if (element_->NeedsStyleRecalc()) {
    DCHECK(DisplayLockUtilities::NearestLockedExclusiveAncestor(*element_));
    return nullptr;
  }

  if (element_->HasDisplayContentsStyle())
    return rejection_names::kUnsupportedDisplay;

  auto* style = element_->GetComputedStyle();
  // Note that if for whatever reason we don't have computed style, then
  // optimistically assume that we have containment.
  if (!style)
    return nullptr;
  if (!style->ContainsStyle() || !style->ContainsLayout())
    return rejection_names::kContainmentNotSatisfied;

  // We allow replaced elements to be locked. This check is similar to the check
  // in DefinitelyNewFormattingContext() in element.cc, but in this case we
  // allow object element to get locked.
  if (IsA<HTMLObjectElement>(*element_) || IsA<HTMLImageElement>(*element_) ||
      element_->IsFormControlElement() || element_->IsMediaElement() ||
      element_->IsFrameOwnerElement() || element_->IsSVGElement()) {
    return nullptr;
  }

  // From https://www.w3.org/TR/css-contain-1/#containment-layout
  // If the element does not generate a principal box (as is the case with
  // display: contents or display: none), or if the element is an internal
  // table element other than display: table-cell, if the element is an
  // internal ruby element, or if the element’s principal box is a
  // non-atomic inline-level box, layout containment has no effect.
  // (Note we're allowing display:none for display locked elements, and a bit
  // more restrictive on ruby - banning <ruby> elements entirely).
  auto* html_element = DynamicTo<HTMLElement>(element_.Get());
  if ((style->IsDisplayTableType() &&
       style->Display() != EDisplay::kTableCell) ||
      (!html_element || IsA<HTMLRubyElement>(html_element)) ||
      (style->IsDisplayInlineType() && !style->IsDisplayReplacedType())) {
    return rejection_names::kContainmentNotSatisfied;
  }
  return nullptr;
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
    is_locked_ = false;
    return true;
  }
  return false;
}

bool DisplayLockContext::ConnectedToView() const {
  return element_ && document_ && element_->isConnected() && document_->View();
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

}  // namespace blink
