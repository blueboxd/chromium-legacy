// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIMELINE_H_

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Document;

class CORE_EXPORT AnimationTimeline : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AnimationTimeline(Document*);
  ~AnimationTimeline() override = default;

  double currentTime(bool& is_null);
  double currentTime();
  base::Optional<double> CurrentTime();
  base::Optional<double> CurrentTimeSeconds();

  virtual bool IsDocumentTimeline() const { return false; }
  virtual bool IsScrollTimeline() const { return false; }
  virtual bool IsActive() const = 0;
  // Returns the initial start time for animations that are linked to this
  // timeline. This method gets invoked when initializing the start time of an
  // animation on this timeline for the first time. It exists because the
  // initial start time for scroll-linked and time-linked animations are
  // different.
  //
  // Changing scroll-linked animation start_time initialization is under
  // consideration here: https://github.com/w3c/csswg-drafts/issues/2075.
  virtual base::Optional<base::TimeDelta> InitialStartTimeForAnimations() = 0;
  Document* GetDocument() { return document_; }
  virtual void AnimationAttached(Animation*);
  virtual void AnimationDetached(Animation*);

  // Updates animation timing.
  virtual void ServiceAnimations(TimingUpdateReason);
  // Schedules next animations timing update.
  virtual void ScheduleNextService() = 0;
  // Schedules animation timing update on next frame.
  virtual void ScheduleServiceOnNextFrame();

  virtual bool NeedsAnimationTimingUpdate();
  virtual bool HasAnimations() const { return !animations_.IsEmpty(); }
  virtual bool HasOutdatedAnimation() const {
    return outdated_animation_count_ > 0;
  }
  void SetOutdatedAnimation(Animation*);
  void ClearOutdatedAnimation(Animation*);

  virtual wtf_size_t AnimationsNeedingUpdateCount() const {
    return animations_needing_update_.size();
  }
  const HeapHashSet<WeakMember<Animation>>& GetAnimations() const {
    return animations_;
  }

  void Trace(blink::Visitor*) override;

 protected:
  virtual base::Optional<base::TimeDelta> CurrentTimeInternal() = 0;
  void RemoveReplacedAnimations();

  Member<Document> document_;
  unsigned outdated_animation_count_;
  // Animations which will be updated on the next frame
  // i.e. current, in effect, or had timing changed
  HeapHashSet<Member<Animation>> animations_needing_update_;
  // All animations attached to this timeline.
  HeapHashSet<WeakMember<Animation>> animations_;

  base::Optional<base::TimeDelta> last_current_time_internal_;
};

}  // namespace blink

#endif
