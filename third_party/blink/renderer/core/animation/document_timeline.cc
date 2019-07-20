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

#include "third_party/blink/renderer/core/animation/document_timeline.h"

#include <algorithm>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/document_timeline_options.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

bool CompareAnimations(const Member<Animation>& left,
                       const Member<Animation>& right) {
  return Animation::HasLowerPriority(left.Get(), right.Get());
}

// Returns the current animation time for a given |document|. This is
// the animation clock time capped to be at least this document's
// ZeroTime() such that the animation time is never negative when converted.
base::TimeTicks CurrentAnimationTime(const Document* document) {
  base::TimeTicks animation_time = document->GetAnimationClock().CurrentTime();
  base::TimeTicks document_zero_time = document->Timeline().ZeroTime();

  // The AnimationClock time may be null or less than the local document's
  // zero time if we have not generated any frames for this document yet. If
  // so, assume animation_time is the document zero time.
  if (animation_time < document_zero_time)
    return document_zero_time;

  return animation_time;
}

}  // namespace

// This value represents 1 frame at 30Hz plus a little bit of wiggle room.
// TODO: Plumb a nominal framerate through and derive this value from that.
const double DocumentTimeline::kMinimumDelay = 0.04;

DocumentTimeline* DocumentTimeline::Create(Document* document,
                                           base::TimeDelta origin_time,
                                           PlatformTiming* timing) {
  return MakeGarbageCollected<DocumentTimeline>(document, origin_time, timing);
}

DocumentTimeline* DocumentTimeline::Create(
    ExecutionContext* execution_context,
    const DocumentTimelineOptions* options) {
  Document* document = To<Document>(execution_context);
  return MakeGarbageCollected<DocumentTimeline>(
      document, base::TimeDelta::FromMillisecondsD(options->originTime()),
      nullptr);
}

DocumentTimeline::DocumentTimeline(Document* document,
                                   base::TimeDelta origin_time,
                                   PlatformTiming* timing)
    : document_(document),
      origin_time_(origin_time),
      zero_time_(base::TimeTicks() + origin_time_),
      zero_time_initialized_(false),
      outdated_animation_count_(0),
      playback_rate_(1),
      last_current_time_internal_(0) {
  if (!timing)
    timing_ = MakeGarbageCollected<DocumentTimelineTiming>(this);
  else
    timing_ = timing;

  if (Platform::Current()->IsThreadedAnimationEnabled())
    compositor_timeline_ = std::make_unique<CompositorAnimationTimeline>();

  DCHECK(document);
}

bool DocumentTimeline::IsActive() const {
  return document_->GetPage();
}

void DocumentTimeline::AnimationAttached(Animation* animation) {
  DCHECK_EQ(&animation->GetDocument()->Timeline(), this);
  DCHECK(!animations_.Contains(animation));
  animations_.insert(animation);
}

Animation* DocumentTimeline::Play(AnimationEffect* child) {
  Animation* animation = Animation::Create(child, this);
  DCHECK(animations_.Contains(animation));

  animation->play();
  DCHECK(animations_needing_update_.Contains(animation));

  return animation;
}

HeapVector<Member<Animation>> DocumentTimeline::getAnimations() {
  // This method implements the Document::getAnimations method defined in the
  // web-animations-1 spec.
  // https://drafts.csswg.org/web-animations-1/#dom-document-getanimations
  document_->UpdateStyleAndLayoutTree();
  HeapVector<Member<Animation>> animations;
  for (const auto& animation : animations_) {
    if (!animation->effect() || (!animation->effect()->IsCurrent() &&
                                 !animation->effect()->IsInEffect())) {
      continue;
    }
    if (animation->effect()->IsKeyframeEffect()) {
      Element* target = ToKeyframeEffect(animation->effect())->target();
      if (!target || !target->isConnected() ||
          document_ != target->GetDocument()) {
        continue;
      }
    }
    animations.push_back(animation);
  }
  std::sort(animations.begin(), animations.end(), CompareAnimations);
  return animations;
}

void DocumentTimeline::Wake() {
  timing_->ServiceOnNextFrame();
}

void DocumentTimeline::ServiceAnimations(TimingUpdateReason reason) {
  TRACE_EVENT0("blink", "DocumentTimeline::serviceAnimations");

  last_current_time_internal_ = CurrentTimeInternal();

  HeapVector<Member<Animation>> animations;
  animations.ReserveInitialCapacity(animations_needing_update_.size());
  for (Animation* animation : animations_needing_update_)
    animations.push_back(animation);

  std::sort(animations.begin(), animations.end(), Animation::HasLowerPriority);

  for (Animation* animation : animations) {
    if (!animation->Update(reason))
      animations_needing_update_.erase(animation);
  }

  DCHECK_EQ(outdated_animation_count_, 0U);
  DCHECK(last_current_time_internal_ == CurrentTimeInternal() ||
         (std::isnan(CurrentTimeInternal()) &&
          std::isnan(last_current_time_internal_)));

#if DCHECK_IS_ON()
  for (const auto& animation : animations_needing_update_)
    DCHECK(!animation->Outdated());
#endif
}

void DocumentTimeline::ScheduleNextService() {
  DCHECK_EQ(outdated_animation_count_, 0U);

  double time_to_next_effect = std::numeric_limits<double>::infinity();
  for (const auto& animation : animations_needing_update_) {
    time_to_next_effect =
        std::min(time_to_next_effect, animation->TimeToEffectChange());
  }

  if (time_to_next_effect < kMinimumDelay) {
    timing_->ServiceOnNextFrame();
  } else if (time_to_next_effect != std::numeric_limits<double>::infinity()) {
    timing_->WakeAfter(time_to_next_effect - kMinimumDelay);
  }
}

void DocumentTimeline::DocumentTimelineTiming::WakeAfter(double duration) {
  base::TimeDelta duration_delta = base::TimeDelta::FromSecondsD(duration);
  if (timer_.IsActive() && timer_.NextFireInterval() < duration_delta)
    return;
  timer_.StartOneShot(duration_delta, FROM_HERE);
}

void DocumentTimeline::DocumentTimelineTiming::ServiceOnNextFrame() {
  if (timeline_->document_->View())
    timeline_->document_->View()->ScheduleAnimation();
}

void DocumentTimeline::DocumentTimelineTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(timeline_);
  DocumentTimeline::PlatformTiming::Trace(visitor);
}

base::TimeTicks DocumentTimeline::ZeroTime() {
  if (!zero_time_initialized_ && document_->Loader()) {
    zero_time_ = document_->Loader()->GetTiming().ReferenceMonotonicTime() +
                 origin_time_;
    zero_time_initialized_ = true;
  }
  return zero_time_;
}

void DocumentTimeline::ResetForTesting() {
  zero_time_ = base::TimeTicks() + origin_time_;
  zero_time_initialized_ = true;
  playback_rate_ = 1;
  last_current_time_internal_ = 0;
}

void DocumentTimeline::SetTimingForTesting(PlatformTiming* timing) {
  timing_ = timing;
}

double DocumentTimeline::currentTime(bool& is_null) {
  return CurrentTimeInternal(is_null) * 1000;
}

// TODO(npm): change the return type to base::Optional<base::TimeTicks>.
double DocumentTimeline::CurrentTimeInternal(bool& is_null) {
  if (!IsActive()) {
    is_null = true;
    return std::numeric_limits<double>::quiet_NaN();
  }

  double result =
      playback_rate_ == 0
          ? ZeroTime().since_origin().InSecondsF()
          : (CurrentAnimationTime(GetDocument()) - ZeroTime()).InSecondsF() *
                playback_rate_;
  is_null = std::isnan(result);
  // This looks like it could never be NaN here.
  DCHECK(!is_null);
  return result;
}

double DocumentTimeline::currentTime() {
  return CurrentTimeInternal() * 1000;
}

double DocumentTimeline::CurrentTimeInternal() {
  bool is_null;
  return CurrentTimeInternal(is_null);
}

double DocumentTimeline::EffectiveTime() {
  double time = CurrentTimeInternal();
  return std::isnan(time) ? 0 : time;
}

void DocumentTimeline::PauseAnimationsForTesting(double pause_time) {
  for (const auto& animation : animations_needing_update_)
    animation->PauseForTesting(pause_time);
  ServiceAnimations(kTimingUpdateOnDemand);
}

bool DocumentTimeline::NeedsAnimationTimingUpdate() {
  if (CurrentTimeInternal() == last_current_time_internal_)
    return false;

  if (std::isnan(CurrentTimeInternal()) &&
      std::isnan(last_current_time_internal_))
    return false;

  // We allow |last_current_time_internal_| to advance here when there
  // are no animations to allow animations spawned during style
  // recalc to not invalidate this flag.
  if (animations_needing_update_.IsEmpty())
    last_current_time_internal_ = CurrentTimeInternal();

  return !animations_needing_update_.IsEmpty();
}

void DocumentTimeline::ClearOutdatedAnimation(Animation* animation) {
  DCHECK(!animation->Outdated());
  outdated_animation_count_--;
}

void DocumentTimeline::SetOutdatedAnimation(Animation* animation) {
  DCHECK(animation->Outdated());
  outdated_animation_count_++;
  animations_needing_update_.insert(animation);
  if (IsActive() && !document_->GetPage()->Animator().IsServicingAnimations())
    timing_->ServiceOnNextFrame();
}

void DocumentTimeline::SetPlaybackRate(double playback_rate) {
  if (!IsActive())
    return;
  double current_time = CurrentTimeInternal();
  playback_rate_ = playback_rate;
  zero_time_ =
      playback_rate == 0
          ? base::TimeTicks() + base::TimeDelta::FromSecondsD(current_time)
          : CurrentAnimationTime(GetDocument()) -
                base::TimeDelta::FromSecondsD(current_time / playback_rate);
  zero_time_initialized_ = true;

  // Corresponding compositor animation may need to be restarted to pick up
  // the new playback rate. Marking the effect changed forces this.
  SetAllCompositorPending(true);
}

void DocumentTimeline::SetAllCompositorPending(bool source_changed) {
  for (const auto& animation : animations_) {
    animation->SetCompositorPending(source_changed);
  }
}

double DocumentTimeline::PlaybackRate() const {
  return playback_rate_;
}

void DocumentTimeline::InvalidateKeyframeEffects(const TreeScope& tree_scope) {
  for (const auto& animation : animations_)
    animation->InvalidateKeyframeEffect(tree_scope);
}

void DocumentTimeline::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(timing_);
  visitor->Trace(animations_needing_update_);
  visitor->Trace(animations_);
  AnimationTimeline::Trace(visitor);
}

}  // namespace blink
