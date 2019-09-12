/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"

#include <algorithm>
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time.h"
#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

static constexpr base::TimeDelta kAnimationPolicyOnceDuration =
    base::TimeDelta::FromSeconds(3);

SMILTimeContainer::SMILTimeContainer(SVGSVGElement& owner)
    : presentation_time_(0),
      reference_time_(0),
      latest_update_time_(0),
      frame_scheduling_state_(kIdle),
      started_(false),
      paused_(false),
      document_order_indexes_dirty_(false),
      intervals_dirty_(true),
      wakeup_timer_(
          owner.GetDocument().GetTaskRunner(TaskType::kInternalDefault),
          this,
          &SMILTimeContainer::WakeupTimerFired),
      animation_policy_once_timer_(
          owner.GetDocument().GetTaskRunner(TaskType::kInternalDefault),
          this,
          &SMILTimeContainer::AnimationPolicyTimerFired),
      owner_svg_element_(&owner) {}

SMILTimeContainer::~SMILTimeContainer() {
  CancelAnimationFrame();
  CancelAnimationPolicyTimer();
  DCHECK(!wakeup_timer_.IsActive());
#if DCHECK_IS_ON()
  DCHECK(!prevent_scheduled_animations_changes_);
#endif
}

void SMILTimeContainer::Schedule(SVGSMILElement* animation,
                                 SVGElement* target,
                                 const QualifiedName& attribute_name) {
  DCHECK_EQ(animation->TimeContainer(), this);
  DCHECK(target);
  DCHECK(animation->HasValidTarget());

#if DCHECK_IS_ON()
  DCHECK(!prevent_scheduled_animations_changes_);
#endif

  // Separate out Discard and AnimateMotion
  QualifiedName name = (animation->HasTagName(svg_names::kAnimateMotionTag) ||
                        animation->HasTagName(svg_names::kDiscardTag))
                           ? animation->TagQName()
                           : attribute_name;

  auto key = std::make_pair(target, name);
  auto& sandwich =
      scheduled_animations_.insert(key, nullptr).stored_value->value;
  if (!sandwich)
    sandwich = MakeGarbageCollected<SMILAnimationSandwich>();

  sandwich->Schedule(animation);

  double latest_update = CurrentDocumentTime();
  if (animation->IntervalBegin() <= latest_update ||
      animation->NextProgressTime(latest_update).IsFinite())
    NotifyIntervalsChanged();
}

void SMILTimeContainer::Unschedule(SVGSMILElement* animation,
                                   SVGElement* target,
                                   const QualifiedName& attribute_name) {
  DCHECK_EQ(animation->TimeContainer(), this);

#if DCHECK_IS_ON()
  DCHECK(!prevent_scheduled_animations_changes_);
#endif

  // Separate out Discard and AnimateMotion
  QualifiedName name = (animation->HasTagName(svg_names::kAnimateMotionTag) ||
                        animation->HasTagName(svg_names::kDiscardTag))
                           ? animation->TagQName()
                           : attribute_name;

  auto key = std::make_pair(target, name);
  AnimationsMap::iterator it = scheduled_animations_.find(key);
  CHECK(it != scheduled_animations_.end());

  auto& sandwich = *(it->value);
  sandwich.Unschedule(animation);

  if (sandwich.IsEmpty())
    scheduled_animations_.erase(it);
}

bool SMILTimeContainer::HasAnimations() const {
  return !scheduled_animations_.IsEmpty();
}

bool SMILTimeContainer::HasPendingSynchronization() const {
  return frame_scheduling_state_ == kSynchronizeAnimations &&
         wakeup_timer_.IsActive() && wakeup_timer_.NextFireInterval().is_zero();
}

void SMILTimeContainer::NotifyIntervalsChanged() {
  intervals_dirty_ = true;
  if (!IsStarted())
    return;
  // Schedule updateAnimations() to be called asynchronously so multiple
  // intervals can change with updateAnimations() only called once at the end.
  if (HasPendingSynchronization())
    return;
  CancelAnimationFrame();
  ScheduleWakeUp(0, kSynchronizeAnimations);
}

double SMILTimeContainer::Elapsed() const {
  if (!IsStarted())
    return 0;

  if (IsPaused())
    return presentation_time_;

  double elapsed = presentation_time_ + (GetDocument()
                                             .Timeline()
                                             .CurrentTimeInternal()
                                             .value_or(base::TimeDelta())
                                             .InSecondsF() -
                                         reference_time_);
  DCHECK_GE(elapsed, 0.0);
  return elapsed;
}

void SMILTimeContainer::ResetDocumentTime() {
  DCHECK(IsStarted());
  // TODO(edvardt): We actually want to check if
  // the document is active and we don't have any special
  // conditions and such, but they require more fixing,
  // probably in SVGSVGElement. I suspect there's a large
  // bug buried here somewhere. This is enough to "paper over"
  // it, but it's not really a solution.
  //
  // Bug: 996196

  SynchronizeToDocumentTimeline();
}

double SMILTimeContainer::CurrentDocumentTime() const {
  return latest_update_time_;
}

void SMILTimeContainer::SynchronizeToDocumentTimeline() {
  reference_time_ = GetDocument()
                        .Timeline()
                        .CurrentTimeInternal()
                        .value_or(base::TimeDelta())
                        .InSecondsF();
}

bool SMILTimeContainer::IsPaused() const {
  // If animation policy is "none", the timeline is always paused.
  return paused_ || AnimationPolicy() == kImageAnimationPolicyNoAnimation;
}

bool SMILTimeContainer::IsStarted() const {
  return started_;
}

bool SMILTimeContainer::IsTimelineRunning() const {
  return IsStarted() && !IsPaused();
}

void SMILTimeContainer::Start() {
  CHECK(!IsStarted());

  if (!GetDocument().IsActive())
    return;

  if (!HandleAnimationPolicy(kRestartOnceTimerIfNotPaused))
    return;

  // Sample the document timeline to get a time reference for the "presentation
  // time".
  SynchronizeToDocumentTimeline();
  started_ = true;

  UpdateAnimationsAndScheduleFrameIfNeeded(presentation_time_);
}

void SMILTimeContainer::Pause() {
  if (!HandleAnimationPolicy(kCancelOnceTimer))
    return;
  DCHECK(!IsPaused());

  if (IsStarted()) {
    presentation_time_ = Elapsed();
    CancelAnimationFrame();
  }
  // Update the flag after sampling elapsed().
  paused_ = true;
}

void SMILTimeContainer::Unpause() {
  if (!HandleAnimationPolicy(kRestartOnceTimer))
    return;
  DCHECK(IsPaused());

  paused_ = false;

  if (!IsStarted())
    return;

  SynchronizeToDocumentTimeline();
  ScheduleWakeUp(0, kSynchronizeAnimations);
}

void SMILTimeContainer::SetElapsed(double elapsed) {
  presentation_time_ = elapsed;

  // If the document hasn't finished loading, |m_presentationTime| will be
  // used as the start time to seek to once it's possible.
  if (!IsStarted())
    return;

  if (!HandleAnimationPolicy(kRestartOnceTimerIfNotPaused))
    return;

  CancelAnimationFrame();

  if (!IsPaused())
    SynchronizeToDocumentTimeline();

#if DCHECK_IS_ON()
  prevent_scheduled_animations_changes_ = true;
#endif
  for (const auto& sandwich : scheduled_animations_) {
    sandwich.value->Reset();
  }
#if DCHECK_IS_ON()
  prevent_scheduled_animations_changes_ = false;
#endif
  intervals_dirty_ = true;
  latest_update_time_ = 0;
  UpdateAnimationsAndScheduleFrameIfNeeded(elapsed);
}

void SMILTimeContainer::ScheduleAnimationFrame(double delay_time) {
  DCHECK(std::isfinite(delay_time));
  DCHECK(IsTimelineRunning());
  DCHECK(!wakeup_timer_.IsActive());

  if (delay_time < DocumentTimeline::kMinimumDelay) {
    ServiceOnNextFrame();
  } else {
    ScheduleWakeUp(delay_time - DocumentTimeline::kMinimumDelay,
                   kFutureAnimationFrame);
  }
}

void SMILTimeContainer::CancelAnimationFrame() {
  frame_scheduling_state_ = kIdle;
  wakeup_timer_.Stop();
}

void SMILTimeContainer::ScheduleWakeUp(
    double delay_time,
    FrameSchedulingState frame_scheduling_state) {
  DCHECK(frame_scheduling_state == kSynchronizeAnimations ||
         frame_scheduling_state == kFutureAnimationFrame);
  wakeup_timer_.StartOneShot(base::TimeDelta::FromSecondsD(delay_time),
                             FROM_HERE);
  frame_scheduling_state_ = frame_scheduling_state;
}

void SMILTimeContainer::WakeupTimerFired(TimerBase*) {
  DCHECK(frame_scheduling_state_ == kSynchronizeAnimations ||
         frame_scheduling_state_ == kFutureAnimationFrame);
  if (frame_scheduling_state_ == kFutureAnimationFrame) {
    DCHECK(IsTimelineRunning());
    frame_scheduling_state_ = kIdle;
    ServiceOnNextFrame();
  } else {
    frame_scheduling_state_ = kIdle;
    UpdateAnimationsAndScheduleFrameIfNeeded(Elapsed());
  }
}

void SMILTimeContainer::ScheduleAnimationPolicyTimer() {
  animation_policy_once_timer_.StartOneShot(kAnimationPolicyOnceDuration,
                                            FROM_HERE);
}

void SMILTimeContainer::CancelAnimationPolicyTimer() {
  animation_policy_once_timer_.Stop();
}

void SMILTimeContainer::AnimationPolicyTimerFired(TimerBase*) {
  Pause();
}

ImageAnimationPolicy SMILTimeContainer::AnimationPolicy() const {
  Settings* settings = GetDocument().GetSettings();
  if (!settings)
    return kImageAnimationPolicyAllowed;

  return settings->GetImageAnimationPolicy();
}

bool SMILTimeContainer::HandleAnimationPolicy(
    AnimationPolicyOnceAction once_action) {
  ImageAnimationPolicy policy = AnimationPolicy();
  // If the animation policy is "none", control is not allowed.
  // returns false to exit flow.
  if (policy == kImageAnimationPolicyNoAnimation)
    return false;
  // If the animation policy is "once",
  if (policy == kImageAnimationPolicyAnimateOnce) {
    switch (once_action) {
      case kRestartOnceTimerIfNotPaused:
        if (IsPaused())
          break;
        FALLTHROUGH;
      case kRestartOnceTimer:
        ScheduleAnimationPolicyTimer();
        break;
      case kCancelOnceTimer:
        CancelAnimationPolicyTimer();
        break;
    }
  }
  if (policy == kImageAnimationPolicyAllowed) {
    // When the SVG owner element becomes detached from its document,
    // the policy defaults to ImageAnimationPolicyAllowed; there's
    // no way back. If the policy had been "once" prior to that,
    // ensure cancellation of its timer.
    if (once_action == kCancelOnceTimer)
      CancelAnimationPolicyTimer();
  }
  return true;
}

void SMILTimeContainer::UpdateDocumentOrderIndexes() {
  unsigned timing_element_count = 0;
  for (SVGSMILElement& element :
       Traversal<SVGSMILElement>::DescendantsOf(OwnerSVGElement()))
    element.SetDocumentOrderIndex(timing_element_count++);
  document_order_indexes_dirty_ = false;
}

SVGSVGElement& SMILTimeContainer::OwnerSVGElement() const {
  return *owner_svg_element_;
}

Document& SMILTimeContainer::GetDocument() const {
  return OwnerSVGElement().GetDocument();
}

void SMILTimeContainer::ServiceOnNextFrame() {
  if (GetDocument().View()) {
    GetDocument().View()->ScheduleAnimation();
    frame_scheduling_state_ = kAnimationFrame;
  }
}

void SMILTimeContainer::ServiceAnimations() {
  if (frame_scheduling_state_ != kAnimationFrame)
    return;

  frame_scheduling_state_ = kIdle;
  UpdateAnimationsAndScheduleFrameIfNeeded(Elapsed());
}

bool SMILTimeContainer::CanScheduleFrame(SMILTime earliest_fire_time) const {
  // If there's synchronization pending (most likely due to syncbases), then
  // let that complete first before attempting to schedule a frame.
  if (HasPendingSynchronization())
    return false;
  if (!IsTimelineRunning())
    return false;
  return earliest_fire_time.IsFinite();
}

void SMILTimeContainer::UpdateAnimationsAndScheduleFrameIfNeeded(
    double elapsed) {
  if (!GetDocument().IsActive())
    return;

  UpdateAnimationTimings(elapsed);
  ApplyAnimationValues(elapsed);

  SMILTime next_progress_time = SMILTime::Unresolved();
  for (auto& sandwich : scheduled_animations_) {
    next_progress_time =
        std::min(next_progress_time, sandwich.value->NextProgressTime(elapsed));
  }

  if (!CanScheduleFrame(next_progress_time))
    return;
  double delay_time = next_progress_time.Value() - elapsed;
  ScheduleAnimationFrame(delay_time);
}

// A helper function to fetch the next interesting time after document_time
SMILTime SMILTimeContainer::NextInterestingTime(
    double presentation_time) const {
  DCHECK_GE(presentation_time, 0);
  SMILTime next_interesting_time = SMILTime::Indefinite();
  for (const auto& sandwich : scheduled_animations_) {
    next_interesting_time =
        std::min(next_interesting_time,
                 sandwich.value->NextInterestingTime(presentation_time));
  }
  DCHECK(!next_interesting_time.IsFinite() ||
         presentation_time < next_interesting_time);
  return next_interesting_time;
}

void SMILTimeContainer::RemoveUnusedKeys() {
  Vector<AnimationId> invalid_keys;
  for (auto& entry : scheduled_animations_) {
    if (entry.value->IsEmpty()) {
      invalid_keys.push_back(entry.key);
    }
  }
  scheduled_animations_.RemoveAll(invalid_keys);
}

void SMILTimeContainer::UpdateIntervals(SMILTime document_time) {
  // This Shrink should free up the memory from the previous
  // run in this loop. Otherwise it won't do anything.
  DCHECK(document_time.IsFinite());
  DCHECK(document_time >= 0.0);
  do {
    intervals_dirty_ = false;
    active_sandwiches_.Shrink(0);
    for (auto& entry : scheduled_animations_) {
      auto* sandwich = entry.value.Get();
      sandwich->UpdateTiming(document_time.Value());

      if (!sandwich->IsEmpty()) {
        active_sandwiches_.push_back(sandwich);
      }
    }

    for (auto& sandwich : active_sandwiches_)
      sandwich->UpdateSyncBases(document_time.Value());
  } while (intervals_dirty_);
}

void SMILTimeContainer::UpdateAnimationTimings(double presentation_time) {
  DCHECK(GetDocument().IsActive());

#if DCHECK_IS_ON()
  // This boolean will catch any attempts to schedule/unschedule
  // scheduledAnimations during this critical section. Similarly, any elements
  // removed will unschedule themselves, so this will catch modification of
  // animationsToApply.
  base::AutoReset<bool> no_scheduled_animations_change_scope(
      &prevent_scheduled_animations_changes_, true);
#endif

  if (document_order_indexes_dirty_)
    UpdateDocumentOrderIndexes();

  RemoveUnusedKeys();

  if (intervals_dirty_)
    UpdateIntervals(latest_update_time_);

  active_sandwiches_.ReserveCapacity(scheduled_animations_.size());
  while (latest_update_time_ < presentation_time) {
    const SMILTime interesting_time = NextInterestingTime(latest_update_time_);
    latest_update_time_ =
        std::min<SMILTime>(presentation_time, interesting_time).Value();

    UpdateIntervals(latest_update_time_);
  }
}

void SMILTimeContainer::ApplyAnimationValues(double elapsed) {
#if DCHECK_IS_ON()
  prevent_scheduled_animations_changes_ = true;
#endif
  HeapVector<Member<SVGSMILElement>> animations_to_apply;
  for (auto& sandwich : active_sandwiches_) {
    sandwich->UpdateActiveAnimationStack(elapsed);
    if (SVGSMILElement* animation = sandwich->ApplyAnimationValues())
      animations_to_apply.push_back(animation);
  }
  active_sandwiches_.Shrink(0);

  if (animations_to_apply.IsEmpty()) {
#if DCHECK_IS_ON()
    prevent_scheduled_animations_changes_ = false;
#endif
    return;
  }

  // Everything bellow handles "discard" elements.
  UseCounter::Count(&GetDocument(), WebFeature::kSVGSMILAnimationAppliedEffect);

  std::sort(animations_to_apply.begin(), animations_to_apply.end(),
            PriorityCompare(elapsed));

#if DCHECK_IS_ON()
  prevent_scheduled_animations_changes_ = false;
#endif

  for (const auto& timed_element : animations_to_apply) {
    if (timed_element->isConnected() && timed_element->IsSVGDiscardElement()) {
      SVGElement* target_element = timed_element->targetElement();
      if (target_element && target_element->isConnected()) {
        UseCounter::Count(&GetDocument(),
                          WebFeature::kSVGSMILDiscardElementTriggered);
        target_element->remove(IGNORE_EXCEPTION_FOR_TESTING);
        DCHECK(!target_element->isConnected());
      }

      if (timed_element->isConnected()) {
        timed_element->remove(IGNORE_EXCEPTION_FOR_TESTING);
        DCHECK(!timed_element->isConnected());
      }
    }
  }
  return;
}

void SMILTimeContainer::AdvanceFrameForTesting() {
  const double kInitialFrameDelay = 0.025;
  SetElapsed(Elapsed() + kInitialFrameDelay);
}

void SMILTimeContainer::Trace(blink::Visitor* visitor) {
  visitor->Trace(scheduled_animations_);
  visitor->Trace(owner_svg_element_);
  visitor->Trace(active_sandwiches_);
}

}  // namespace blink
