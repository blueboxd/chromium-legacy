// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

AnimationTimeline::AnimationTimeline(Document* document)
    : document_(document), outdated_animation_count_(0) {
  document_->GetDocumentAnimations().AddTimeline(*this);
}

void AnimationTimeline::AnimationAttached(Animation* animation) {
  DCHECK(!animations_.Contains(animation));
  animations_.insert(animation);
}

void AnimationTimeline::AnimationDetached(Animation* animation) {
  animations_.erase(animation);
  animations_needing_update_.erase(animation);
  if (animation->Outdated())
    outdated_animation_count_--;
}

double AnimationTimeline::currentTime(bool& is_null) {
  base::Optional<base::TimeDelta> result = CurrentTimeInternal();

  is_null = !result.has_value();
  return result ? result->InMillisecondsF()
                : std::numeric_limits<double>::quiet_NaN();
}

double AnimationTimeline::currentTime() {
  base::Optional<base::TimeDelta> result = CurrentTimeInternal();
  return result ? result->InMillisecondsF()
                : std::numeric_limits<double>::quiet_NaN();
}

base::Optional<double> AnimationTimeline::CurrentTime() {
  base::Optional<base::TimeDelta> result = CurrentTimeInternal();
  return result ? base::make_optional(result->InMillisecondsF())
                : base::nullopt;
}

base::Optional<double> AnimationTimeline::CurrentTimeSeconds() {
  base::Optional<base::TimeDelta> result = CurrentTimeInternal();
  return result ? base::make_optional(result->InSecondsF()) : base::nullopt;
}

void AnimationTimeline::ClearOutdatedAnimation(Animation* animation) {
  DCHECK(!animation->Outdated());
  outdated_animation_count_--;
}

bool AnimationTimeline::NeedsAnimationTimingUpdate() {
  if (CurrentTimeInternal() == last_current_time_internal_)
    return false;

  // We allow |last_current_time_internal_| to advance here when there
  // are no animations to allow animations spawned during style
  // recalc to not invalidate this flag.
  if (animations_needing_update_.IsEmpty())
    last_current_time_internal_ = CurrentTimeInternal();

  return !animations_needing_update_.IsEmpty();
}

void AnimationTimeline::ServiceAnimations(TimingUpdateReason reason) {
  TRACE_EVENT0("blink", "AnimationTimeline::serviceAnimations");

  last_current_time_internal_ = CurrentTimeInternal();

  HeapVector<Member<Animation>> animations;
  animations.ReserveInitialCapacity(animations_needing_update_.size());
  for (Animation* animation : animations_needing_update_)
    animations.push_back(animation);

  std::sort(animations.begin(), animations.end(),
            Animation::HasLowerCompositeOrdering);

  for (Animation* animation : animations) {
    if (!animation->Update(reason))
      animations_needing_update_.erase(animation);
  }

  DCHECK_EQ(outdated_animation_count_, 0U);
  DCHECK(last_current_time_internal_ == CurrentTimeInternal());

#if DCHECK_IS_ON()
  for (const auto& animation : animations_needing_update_)
    DCHECK(!animation->Outdated());
#endif
  // Explicitly free the backing store to avoid memory regressions.
  // TODO(bikineev): Revisit when young generation is done.
  animations.clear();
}

void AnimationTimeline::SetOutdatedAnimation(Animation* animation) {
  DCHECK(animation->Outdated());
  outdated_animation_count_++;
  animations_needing_update_.insert(animation);
  if (IsActive() && !document_->GetPage()->Animator().IsServicingAnimations())
    ScheduleServiceOnNextFrame();
}

void AnimationTimeline::ScheduleServiceOnNextFrame() {
  if (document_->View())
    document_->View()->ScheduleAnimation();
}

void AnimationTimeline::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(animations_needing_update_);
  visitor->Trace(animations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
