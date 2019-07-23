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

SMILAnimationSandwich::SMILAnimationSandwich() {}

void SMILAnimationSandwich::Schedule(SVGSMILElement* animation) {
  DCHECK(!sandwich_.Contains(animation));
  sandwich_.push_back(animation);
}

void SMILAnimationSandwich::Unschedule(SVGSMILElement* animation) {
  auto* position = std::find(sandwich_.begin(), sandwich_.end(), animation);
  DCHECK(sandwich_.end() != position);
  sandwich_.erase(position);
}

void SMILAnimationSandwich::Reset() {
  for (SVGSMILElement* animation : sandwich_) {
    animation->Reset();
  }
}

void SMILAnimationSandwich::UpdateTiming(double elapsed, bool seek_to_time) {
  if (!std::is_sorted(sandwich_.begin(), sandwich_.end(),
                      PriorityCompare(elapsed))) {
    std::sort(sandwich_.begin(), sandwich_.end(), PriorityCompare(elapsed));
  }

  active_.ReserveCapacity(sandwich_.size());
  for (const auto& it_animation : sandwich_) {
    SVGSMILElement* animation = it_animation.Get();
    DCHECK(animation->HasValidTarget());

    if (animation->NeedsToProgress(elapsed)) {
      animation->Progress(elapsed, seek_to_time);
      active_.push_back(animation);
    } else if (animation->IsContributing(elapsed)) {
      active_.push_back(animation);
    } else {
      animation->ClearAnimatedType();
    }
  }
}

SMILTime SMILAnimationSandwich::GetNextFireTime() {
  SMILTime earliest_fire_time = SMILTime::Unresolved();
  for (const auto& it_animation : sandwich_) {
    SVGSMILElement* animation = it_animation.Get();

    SMILTime next_fire_time = animation->NextProgressTime();
    if (next_fire_time.IsFinite())
      earliest_fire_time = std::min(next_fire_time, earliest_fire_time);
  }
  return earliest_fire_time;
}

void SMILAnimationSandwich::SendEvents(double elapsed, bool seek_to_time) {
  if (seek_to_time) {
    for (auto& animation : active_) {
      animation->TriggerPendingEvents(elapsed);
    }
  }

  for (auto& animation : active_) {
    animation->UpdateSyncbases();
  }

  for (auto& animation : active_) {
    animation->UpdateNextProgressTime(elapsed);
  }

  auto* it = active_.begin();
  while (it != active_.end()) {
    auto* scheduled = it->Get();
    if (scheduled->IsContributing(elapsed)) {
      it++;
      continue;
    }
    scheduled->ClearAnimatedType();
    it = active_.erase(it);
  }
}

SVGSMILElement* SMILAnimationSandwich::ApplyAnimationValues() {
  if (active_.IsEmpty())
    return nullptr;
  // Results are accumulated to the first animation that animates and
  // contributes to a particular element/attribute pair.
  // Only reset the animated type to the base value once for
  // the lowest priority animation that animates and
  // contributes to a particular element/attribute pair.
  SVGSMILElement* result_element = active_.front();
  result_element->ResetAnimatedType();

  // Animations have to be applied lowest to highest prio.
  //
  // Only calculate the relevant animations. If we actually set the
  // animation value, we don't need to calculate what is beneath it
  // in the sandwich.
  auto* sandwich_start = active_.end();
  while (sandwich_start != active_.begin()) {
    --sandwich_start;
    if ((*sandwich_start)->OverwritesUnderlyingAnimationValue())
      break;
  }

  for (auto* sandwich_it = sandwich_start; sandwich_it != active_.end();
       sandwich_it++) {
    (*sandwich_it)->UpdateAnimatedValue(result_element);
  }
  active_.Shrink(0);

  result_element->ApplyResultsToTarget();

  return result_element;
}

void SMILAnimationSandwich::Trace(blink::Visitor* visitor) {
  visitor->Trace(sandwich_);
  visitor->Trace(active_);
}

}  // namespace blink
