// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"

#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"

namespace blink {

void SequencedScroll::Trace(blink::Visitor* visitor) {
  visitor->Trace(scrollable_area);
}

void SmoothScrollSequencer::QueueAnimation(
    ScrollableArea* scrollable,
    ScrollOffset offset,
    mojom::blink::ScrollIntoViewParams::Behavior behavior) {
  if (scrollable->ClampScrollOffset(offset) != scrollable->GetScrollOffset()) {
    queue_.push_back(
        MakeGarbageCollected<SequencedScroll>(scrollable, offset, behavior));
  }
}

void SmoothScrollSequencer::RunQueuedAnimations() {
  if (queue_.IsEmpty()) {
    current_scrollable_ = nullptr;
    scroll_type_ = kProgrammaticScroll;
    return;
  }
  SequencedScroll* sequenced_scroll = queue_.back();
  queue_.pop_back();
  current_scrollable_ = sequenced_scroll->scrollable_area;
  current_scrollable_->SetScrollOffset(sequenced_scroll->scroll_offset,
                                       kSequencedScroll,
                                       sequenced_scroll->scroll_behavior);
}

void SmoothScrollSequencer::AbortAnimations() {
  if (current_scrollable_) {
    current_scrollable_->CancelProgrammaticScrollAnimation();
    current_scrollable_ = nullptr;
  }
  queue_.clear();
  scroll_type_ = kProgrammaticScroll;
}

bool SmoothScrollSequencer::FilterNewScrollOrAbortCurrent(
    ScrollType incoming_type) {
  // Allow the incoming scroll to co-exist if its scroll type is
  // kSequencedScroll, kClampingScroll, or kAnchoringScroll
  if (incoming_type == kSequencedScroll || incoming_type == kClampingScroll ||
      incoming_type == kAnchoringScroll)
    return false;

  // If the current sequenced scroll is UserScroll, but the incoming scroll is
  // not, filter the incoming scroll. See crbug.com/913009 for more details.
  if (scroll_type_ == kUserScroll && incoming_type != kUserScroll)
    return true;

  // Otherwise, abort the current sequenced scroll.
  AbortAnimations();
  return false;
}

void SmoothScrollSequencer::DidDisposeScrollableArea(
    const ScrollableArea& area) {
  for (Member<SequencedScroll>& sequenced_scroll : queue_) {
    if (sequenced_scroll->scrollable_area.Get() == &area) {
      AbortAnimations();
      break;
    }
  }
}

void SmoothScrollSequencer::Trace(blink::Visitor* visitor) {
  visitor->Trace(queue_);
  visitor->Trace(current_scrollable_);
}

}  // namespace blink
