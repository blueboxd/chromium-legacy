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

SMILAnimationSandwich::SMILAnimationSandwich() = default;

void SMILAnimationSandwich::Schedule(SVGSMILElement* animation) {
  DCHECK(!sandwich_.Contains(animation));
  sandwich_.push_back(animation);
}

void SMILAnimationSandwich::Unschedule(SVGSMILElement* animation) {
  auto* position = std::find(sandwich_.begin(), sandwich_.end(), animation);
  DCHECK(sandwich_.end() != position);
  sandwich_.erase(position);
  if (animation == ResultElement())
    animation->ClearAnimatedType();
}

void SMILAnimationSandwich::Reset() {
  if (SVGSMILElement* result_element = ResultElement())
    result_element->ClearAnimatedType();
  for (SVGSMILElement* animation : sandwich_)
    animation->Reset();
}

void SMILAnimationSandwich::UpdateTiming(SMILTime elapsed) {
  if (!std::is_sorted(sandwich_.begin(), sandwich_.end(),
                      PriorityCompare(elapsed))) {
    std::sort(sandwich_.begin(), sandwich_.end(), PriorityCompare(elapsed));
  }

  for (const auto& animation : sandwich_) {
    DCHECK(animation->HasValidTarget());

    if (!animation->NeedsToProgress(elapsed))
      continue;
    bool interval_restart = animation->CheckAndUpdateInterval(elapsed);
    animation->UpdateActiveState(elapsed, interval_restart);
  }
}

SMILTime SMILAnimationSandwich::NextInterestingTime(
    SMILTime presentation_time) const {
  SMILTime interesting_time = SMILTime::Indefinite();
  for (const auto& animation : sandwich_) {
    interesting_time = std::min(
        interesting_time, animation->NextInterestingTime(presentation_time));
  }
  return interesting_time;
}

SMILTime SMILAnimationSandwich::NextProgressTime(
    SMILTime presentation_time) const {
  SMILTime earliest_progress_time = SMILTime::Unresolved();
  for (const auto& animation : sandwich_) {
    earliest_progress_time = std::min(
        earliest_progress_time, animation->NextProgressTime(presentation_time));
    if (earliest_progress_time <= presentation_time)
      break;
  }
  return earliest_progress_time;
}

void SMILAnimationSandwich::UpdateSyncBases(SMILTime elapsed) {
  for (auto& animation : sandwich_)
    animation->UpdateSyncBases();
}

SVGSMILElement* SMILAnimationSandwich::ResultElement() const {
  return !active_.IsEmpty() ? active_.front() : nullptr;
}

void SMILAnimationSandwich::UpdateActiveAnimationStack(
    SMILTime presentation_time) {
  SVGSMILElement* old_result_element = ResultElement();
  active_.Shrink(0);
  active_.ReserveCapacity(sandwich_.size());
  // Build the contributing/active sandwich.
  for (auto& animation : sandwich_) {
    if (!animation->IsContributing(presentation_time))
      continue;
    active_.push_back(animation);
  }
  // If we switched result element, clear the old one.
  if (old_result_element && old_result_element != ResultElement())
    old_result_element->ClearAnimatedType();
}

SVGSMILElement* SMILAnimationSandwich::ApplyAnimationValues() {
  SVGSMILElement* result_element = ResultElement();
  if (!result_element)
    return nullptr;

  // Only reset the animated type to the base value once for
  // the lowest priority animation that animates and
  // contributes to a particular element/attribute pair.
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

  result_element->ApplyResultsToTarget();
  return result_element;
}

void SMILAnimationSandwich::Trace(blink::Visitor* visitor) {
  visitor->Trace(sandwich_);
  visitor->Trace(active_);
}

}  // namespace blink
