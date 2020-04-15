// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"

namespace blink {

DisplayLockDocumentState::DisplayLockDocumentState(Document* document)
    : document_(document) {}

void DisplayLockDocumentState::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(display_lock_contexts_);
}

void DisplayLockDocumentState::AddDisplayLockContext(
    DisplayLockContext* context) {
  display_lock_contexts_.insert(context);
}

void DisplayLockDocumentState::RemoveDisplayLockContext(
    DisplayLockContext* context) {
  display_lock_contexts_.erase(context);
}

int DisplayLockDocumentState::DisplayLockCount() const {
  return display_lock_contexts_.size();
}

void DisplayLockDocumentState::AddLockedDisplayLock() {
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
                    "LockedDisplayLockCount", TRACE_ID_LOCAL(this),
                    locked_display_lock_count_);
  ++locked_display_lock_count_;
}

void DisplayLockDocumentState::RemoveLockedDisplayLock() {
  DCHECK(locked_display_lock_count_);
  --locked_display_lock_count_;
  TRACE_COUNTER_ID1(TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"),
                    "LockedDisplayLockCount", TRACE_ID_LOCAL(this),
                    locked_display_lock_count_);
}

int DisplayLockDocumentState::LockedDisplayLockCount() const {
  return locked_display_lock_count_;
}

void DisplayLockDocumentState::IncrementDisplayLockBlockingAllActivation() {
  ++display_lock_blocking_all_activation_count_;
}

void DisplayLockDocumentState::DecrementDisplayLockBlockingAllActivation() {
  DCHECK(display_lock_blocking_all_activation_count_);
  --display_lock_blocking_all_activation_count_;
}

int DisplayLockDocumentState::DisplayLockBlockingAllActivationCount() const {
  return display_lock_blocking_all_activation_count_;
}

void DisplayLockDocumentState::RegisterDisplayLockActivationObservation(
    Element* element) {
  EnsureIntersectionObserver().observe(element);
}

void DisplayLockDocumentState::UnregisterDisplayLockActivationObservation(
    Element* element) {
  EnsureIntersectionObserver().unobserve(element);
}

IntersectionObserver& DisplayLockDocumentState::EnsureIntersectionObserver() {
  if (!intersection_observer_) {
    // Use kDeliverDuringPostLifecycleSteps method, since we delay delivering
    // the signal to the display lock context until the next frame's rAF
    // callbacks have run. This means for the duration of the idle time that
    // follows, we won't dirty layout.
    //
    // Note that we use 50% margin (on the viewport) so that we get the
    // observation before the element enters the viewport.
    intersection_observer_ = IntersectionObserver::Create(
        {Length::Percent(50.f)}, {std::numeric_limits<float>::min()}, document_,
        WTF::BindRepeating(
            &DisplayLockDocumentState::ProcessDisplayLockActivationObservation,
            WrapWeakPersistent(this)),
        IntersectionObserver::kDeliverDuringPostLifecycleSteps);
  }
  return *intersection_observer_;
}

void DisplayLockDocumentState::ProcessDisplayLockActivationObservation(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(document_);
  DCHECK(document_->View());
  for (auto& entry : entries) {
    auto* context = entry->target()->GetDisplayLockContext();
    DCHECK(context);
    if (entry->isIntersecting()) {
      document_->View()->EnqueueStartOfLifecycleTask(
          WTF::Bind(&DisplayLockContext::NotifyIsIntersectingViewport,
                    WrapWeakPersistent(context)));
    } else {
      document_->View()->EnqueueStartOfLifecycleTask(
          WTF::Bind(&DisplayLockContext::NotifyIsNotIntersectingViewport,
                    WrapWeakPersistent(context)));
    }
  }
  document_->View()->ScheduleAnimation();
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks
DisplayLockDocumentState::GetScopedForceActivatableLocks() {
  return ScopedForceActivatableDisplayLocks(this);
}

bool DisplayLockDocumentState::ActivatableDisplayLocksForced() const {
  return activatable_display_locks_forced_;
}

void DisplayLockDocumentState::NotifySelectionRemoved() {
  for (auto context : display_lock_contexts_)
    context->NotifySubtreeLostSelection();
}

void DisplayLockDocumentState::BeginNodeForcedScope(
    const Node* node,
    bool self_was_forced,
    DisplayLockUtilities::ScopedChainForcedUpdate* scope) {
  forced_node_info_.emplace_back(node, self_was_forced, scope);
}

void DisplayLockDocumentState::EndNodeForcedScope(
    DisplayLockUtilities::ScopedChainForcedUpdate* scope) {
  for (wtf_size_t i = 0; i < forced_node_info_.size(); ++i) {
    if (forced_node_info_[i].scope == scope) {
      forced_node_info_.EraseAt(i);
      return;
    }
  }
  // We should always find a scope to erase.
  NOTREACHED();
}

void DisplayLockDocumentState::ForceLockIfNeeded(Element* element) {
  DCHECK(element->GetDisplayLockContext());
  for (wtf_size_t i = 0; i < forced_node_info_.size(); ++i)
    ForceLockIfNeededForInfo(element, &forced_node_info_[i]);
}

void DisplayLockDocumentState::ForceLockIfNeededForInfo(
    Element* element,
    ForcedNodeInfo* forced_node_info) {
  auto ancestor_view =
      forced_node_info->self_forced
          ? FlatTreeTraversal::InclusiveAncestorsOf(*forced_node_info->node)
          : FlatTreeTraversal::AncestorsOf(*forced_node_info->node);
  for (Node& ancestor : ancestor_view) {
    if (element == &ancestor) {
      forced_node_info->scope->AddScopedForcedUpdate(
          element->GetDisplayLockContext());
      break;
    }
  }
}

// ScopedForcedActivatableDisplayLocks implementation -----------
DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::
    ScopedForceActivatableDisplayLocks(DisplayLockDocumentState* state)
    : state_(state) {
  if (++state_->activatable_display_locks_forced_ == 1) {
    for (auto context : state_->display_lock_contexts_)
      context->DidForceActivatableDisplayLocks();
  }
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::
    ScopedForceActivatableDisplayLocks(
        ScopedForceActivatableDisplayLocks&& other)
    : state_(other.state_) {
  other.state_ = nullptr;
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks&
DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::operator=(
    ScopedForceActivatableDisplayLocks&& other) {
  state_ = other.state_;
  other.state_ = nullptr;
  return *this;
}

DisplayLockDocumentState::ScopedForceActivatableDisplayLocks::
    ~ScopedForceActivatableDisplayLocks() {
  if (!state_)
    return;
  DCHECK(state_->activatable_display_locks_forced_);
  --state_->activatable_display_locks_forced_;
}

}  // namespace blink
