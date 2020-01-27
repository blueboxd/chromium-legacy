// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
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

  if (RuntimeEnabledFeatures::WebAnimationsAPIEnabled() &&
      reason == kTimingUpdateForAnimationFrame) {
    RemoveReplacedAnimations();
  }
}

// https://drafts.csswg.org/web-animations-1/#removing-replaced-animations
void AnimationTimeline::RemoveReplacedAnimations() {
  // Group replaceable animations by target element.
  HeapHashMap<Member<Element>, Member<HeapVector<Member<Animation>>>>
      replaceable_animations;
  for (Animation* animation : animations_) {
    // Initial conditions for removal:
    // * has an associated animation effect whose effect target is a descendant
    //    of doc, and
    // * is replaceable
    if (!animation->IsReplaceable())
      continue;
    DCHECK(animation->effect());
    Element* target = To<KeyframeEffect>(animation->effect())->target();
    DCHECK(target);
    if (target->GetDocument() != animation->GetDocument())
      continue;

    auto inserted = replaceable_animations.insert(target, nullptr);
    if (inserted.is_new_entry) {
      inserted.stored_value->value =
          MakeGarbageCollected<HeapVector<Member<Animation>>>();
    }
    inserted.stored_value->value->push_back(animation);
  }

  HeapVector<Member<Animation>> animations_to_remove;
  for (auto& elem_it : replaceable_animations) {
    HeapVector<Member<Animation>>* animations = elem_it.value;

    // Only elements with multiple animations in the replaceable state need to
    // be checked.
    if (animations->size() == 1)
      continue;

    // By processing in decreasing order by priority, we can perform a single
    // pass for discovery of replaced properties.
    std::sort(animations->begin(), animations->end(),
              Animation::HasLowerCompositeOrdering);
    PropertyHandleSet replaced_properties;
    for (auto anim_it = animations->rbegin(); anim_it != animations->rend();
         anim_it++) {
      // Remaining conditions for removal:
      // * has a replace state of active,  and
      // * for which there exists for each target property of every animation
      //   effect associated with animation, an animation effect associated with
      //   a replaceable animation with a higher composite order than animation
      //   that includes the same target property.

      // Only active animations can be removed. We still need to go through
      // the process of iterating over properties if not removable to update
      // the set of properties being replaced.
      bool replace = (*anim_it)->ReplaceStateActive();
      PropertyHandleSet animation_properties =
          To<KeyframeEffect>((*anim_it)->effect())->Model()->Properties();
      for (const auto& property : animation_properties) {
        auto inserted = replaced_properties.insert(property);
        if (inserted.is_new_entry) {
          // Top-most compositor order animation affecting this property.
          replace = false;
        }
      }
      if (replace)
        animations_to_remove.push_back(*anim_it);
    }
  }

  // The list of animations for removal is constructed in reverse composite
  // ordering for efficiency. Flip the ordering to ensure that events are
  // dispatched in composite order.
  // TODO(crbug.com/981905): Add test for ordering once onremove is implemented.
  for (auto it = animations_to_remove.rbegin();
       it != animations_to_remove.rend(); it++) {
    (*it)->RemoveReplacedAnimation();
  }
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
