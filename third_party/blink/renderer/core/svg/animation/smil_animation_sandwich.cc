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

#include "third_party/blink/renderer/core/svg/animation/smil_animation_sandwich.h"

#include <algorithm>

#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"

namespace blink {

SMILAnimationSandwich::SMILAnimationSandwich(
    const ScheduledVector& scheduled_elements,
    double elapsed,
    bool seek_to_time)
    : earliest_fire_time_(SMILTime::Unresolved()),
      elapsed_(elapsed),
      seek_to_time_(seek_to_time) {
  for (const auto& it_animation : scheduled_elements) {
    SVGSMILElement* animation = it_animation.Get();
    DCHECK(animation->HasValidTarget());

    if (animation->NeedsToProgress(elapsed_, seek_to_time_)) {
      animation->Progress(elapsed_, seek_to_time_);
      sandwich_.push_back(animation);
    } else if (animation->IsContributing(elapsed_)) {
      sandwich_.push_back(animation);
    } else {
      animation->ClearAnimatedType();
    }

    SMILTime next_fire_time = animation->NextProgressTime();
    if (next_fire_time.IsFinite())
      earliest_fire_time_ = std::min(next_fire_time, earliest_fire_time_);
  }
}

SMILTime SMILAnimationSandwich::GetNextFireTime() {
  return earliest_fire_time_;
}

void SMILAnimationSandwich::UpdateAnimations() {
  if (seek_to_time_) {
    for (auto& animation : sandwich_) {
      animation->TriggerPendingEvents(elapsed_);
    }
  }

  for (auto& animation : sandwich_) {
    animation->UpdateSyncbases();
  }

  for (auto& animation : sandwich_) {
    animation->UpdateNextProgressTime(elapsed_);
  }

  auto* it = sandwich_.begin();
  while (it != sandwich_.end()) {
    auto* scheduled = it->Get();
    if (scheduled->IsContributing(elapsed_)) {
      it++;
      continue;
    }
    scheduled->ClearAnimatedType();
    it = sandwich_.erase(it);
  }

  for (auto& animation : sandwich_) {
    SMILTime next_fire_time = animation->NextProgressTime();
    if (next_fire_time.IsFinite())
      earliest_fire_time_ = std::min(next_fire_time, earliest_fire_time_);
  }
}

SVGSMILElement* SMILAnimationSandwich::UpdateAnimationValues() {
  // Results are accumulated to the first animation that animates and
  // contributes to a particular element/attribute pair.
  // Only reset the animated type to the base value once for
  // the lowest priority animation that animates and
  // contributes to a particular element/attribute pair.
  SVGSMILElement* result_element = sandwich_.front();
  result_element->ResetAnimatedType();

  // Animations have to be applied lowest to highest prio.
  //
  // Only calculate the relevant animations. If we actually set the
  // animation value, we don't need to calculate what is beneath it
  // in the sandwich.
  auto* sandwich_start = sandwich_.end();
  while (sandwich_start != sandwich_.begin()) {
    --sandwich_start;
    if ((*sandwich_start)->OverwritesUnderlyingAnimationValue())
      break;
  }

  for (auto* sandwich_it = sandwich_start; sandwich_it != sandwich_.end();
       sandwich_it++) {
    (*sandwich_it)->UpdateAnimatedValue(result_element);
  }

  return result_element;
}

void SMILAnimationSandwich::Trace(blink::Visitor* visitor) {
  visitor->Trace(sandwich_);
}

}  // namespace blink
