/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/document_animations.h"

#include "cc/animation/animation_host.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"

namespace blink {

namespace {

void UpdateAnimationTiming(
    Document& document,
    HeapHashSet<WeakMember<AnimationTimeline>>& timelines,
    TimingUpdateReason reason) {
  for (auto& timeline : timelines)
    timeline->ServiceAnimations(reason);
  document.GetWorkletAnimationController().UpdateAnimationTimings(reason);
}

bool CompareAnimations(const Member<Animation>& left,
                       const Member<Animation>& right) {
  return Animation::HasLowerCompositeOrdering(
      left.Get(), right.Get(),
      Animation::CompareAnimationsOrdering::kTreeOrder);
}
}  // namespace

DocumentAnimations::DocumentAnimations(Document* document)
    : document_(document) {}

void DocumentAnimations::AddTimeline(AnimationTimeline& timeline) {
  timelines_.insert(&timeline);
}

void DocumentAnimations::UpdateAnimationTimingForAnimationFrame() {
  UpdateAnimationTiming(*document_, timelines_, kTimingUpdateForAnimationFrame);
}

bool DocumentAnimations::NeedsAnimationTimingUpdate() {
  for (auto& timeline : timelines_) {
    if (timeline->HasOutdatedAnimation() ||
        timeline->NeedsAnimationTimingUpdate())
      return true;
  }
  return false;
}

void DocumentAnimations::UpdateAnimationTimingIfNeeded() {
  if (NeedsAnimationTimingUpdate())
    UpdateAnimationTiming(*document_, timelines_, kTimingUpdateOnDemand);
}

void DocumentAnimations::UpdateAnimations(
    DocumentLifecycle::LifecycleState required_lifecycle_state,
    const PaintArtifactCompositor* paint_artifact_compositor) {
  DCHECK(document_->Lifecycle().GetState() >= required_lifecycle_state);

  if (document_->GetPendingAnimations().Update(paint_artifact_compositor)) {
    DCHECK(document_->View());
    document_->View()->ScheduleAnimation();
  }
  if (document_->View()) {
    if (cc::AnimationHost* host =
            document_->View()->GetCompositorAnimationHost()) {
      wtf_size_t total_animations_count = 0;
      for (auto& timeline : timelines_) {
        if (timeline->HasAnimations())
          total_animations_count += timeline->AnimationsNeedingUpdateCount();
      }

      // In the CompositorTimingHistory::DidDraw where we know that there is
      // visual update, we will use document.CurrentFrameHadRAF as a signal to
      // record UMA or not.
      host->SetAnimationCounts(total_animations_count,
                               document_->CurrentFrameHadRAF(),
                               document_->NextFrameHasPendingRAF());
    }
  }

  document_->GetWorkletAnimationController().UpdateAnimationStates();
  for (auto& timeline : timelines_)
    timeline->ScheduleNextService();
}

HeapVector<Member<Animation>> DocumentAnimations::getAnimations(
    const TreeScope& tree_scope) {
  // This method implements the Document::getAnimations method defined in the
  // web-animations-1 spec.
  // https://drafts.csswg.org/web-animations-1/#dom-document-getanimations
  // TODO(crbug.com/1046916): refactoring work to create a shared implementation
  // of getAnimations for Documents and ShadowRoots.
  document_->UpdateStyleAndLayoutTree();
  HeapVector<Member<Animation>> animations;
  if (document_->GetPage())
    animations = document_->GetPage()->Animator().GetAnimations(tree_scope);
  else
    GetAnimationsTargetingTreeScope(animations, tree_scope);

  std::sort(animations.begin(), animations.end(), CompareAnimations);
  return animations;
}

void DocumentAnimations::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(timelines_);
}

void DocumentAnimations::GetAnimationsTargetingTreeScope(
    HeapVector<Member<Animation>>& animations,
    const TreeScope& tree_scope) {
  // This method follows the timelines in a given docmuent and append all the
  // animations to the reference animations.
  for (auto& timeline : timelines_) {
    for (const auto& animation : timeline->GetAnimations()) {
      if (animation->ReplaceStateRemoved())
        continue;
      if (!animation->effect() || (!animation->effect()->IsCurrent() &&
                                   !animation->effect()->IsInEffect())) {
        continue;
      }
      auto* effect = DynamicTo<KeyframeEffect>(animation->effect());
      Element* target = effect->target();
      if (!target || !target->isConnected())
        continue;
      if (&tree_scope != &target->GetTreeScope())
        continue;
      animations.push_back(animation);
    }
  }
}
}  // namespace blink
