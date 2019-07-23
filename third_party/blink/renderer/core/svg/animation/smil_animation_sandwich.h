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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_ANIMATION_SANDWICH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_ANIMATION_SANDWICH_H_

#include "third_party/blink/renderer/core/svg/animation/smil_time.h"
#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct PriorityCompare {
  PriorityCompare(double elapsed) : elapsed_(elapsed) {}
  bool operator()(const Member<SVGSMILElement>& a,
                  const Member<SVGSMILElement>& b) {
    // FIXME: This should also consider possible timing relations between the
    // elements.
    SMILTime a_begin = a->IntervalBegin();
    SMILTime b_begin = b->IntervalBegin();
    // Frozen elements need to be prioritized based on their previous interval.
    a_begin = a->IsFrozen() && elapsed_ < a_begin ? a->PreviousIntervalBegin()
                                                  : a_begin;
    b_begin = b->IsFrozen() && elapsed_ < b_begin ? b->PreviousIntervalBegin()
                                                  : b_begin;
    if (a_begin == b_begin)
      return a->DocumentOrderIndex() < b->DocumentOrderIndex();
    return a_begin < b_begin;
  }
  double elapsed_;
};

class SMILAnimationSandwich : public GarbageCollected<SMILAnimationSandwich> {
 public:
  using ScheduledVector = HeapVector<Member<SVGSMILElement>>;
  explicit SMILAnimationSandwich();

  void Schedule(SVGSMILElement* animation);
  void Unschedule(SVGSMILElement* animation);
  void Reset();

  void UpdateTiming(double elapsed, bool seek_to_time);
  void SendEvents(double elapsed, bool seek_to_time);
  SVGSMILElement* ApplyAnimationValues();

  SMILTime GetNextFireTime();

  bool IsEmpty() { return sandwich_.IsEmpty(); }

  void Trace(blink::Visitor*);

 private:
  // The list stored here is always sorted.
  ScheduledVector sandwich_;
  ScheduledVector active_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_ANIMATION_SANDWICH_H_
