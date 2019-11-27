// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_EVENTS_H_
#define CC_ANIMATION_ANIMATION_EVENTS_H_

#include <memory>
#include <vector>

#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/trees/mutator_host.h"

namespace cc {

struct CC_ANIMATION_EXPORT AnimationEvent {
  enum Type { STARTED, FINISHED, ABORTED, TAKEOVER, TIME_UPDATED };

  typedef size_t KeyframeEffectId;
  struct UniqueKeyframeModelId {
    int timeline_id;
    int animation_id;
    KeyframeEffectId effect_id;
    int model_id;
  };

  AnimationEvent(Type type,
                 UniqueKeyframeModelId uid,
                 int group_id,
                 int target_property,
                 base::TimeTicks monotonic_time);

  // Constructs AnimationEvent of TIME_UPDATED type.
  AnimationEvent(int timeline_id,
                 int animation_id,
                 base::Optional<base::TimeDelta> local_time);

  AnimationEvent(const AnimationEvent& other);
  AnimationEvent& operator=(const AnimationEvent& other);

  ~AnimationEvent();

  bool ShouldDispatchToKeyframeEffectAndModel() const;

  Type type;
  UniqueKeyframeModelId uid;
  int group_id;
  int target_property;
  base::TimeTicks monotonic_time;
  bool is_impl_only;

  // For continuing a scroll offset animation on the main thread.
  base::TimeTicks animation_start_time;
  std::unique_ptr<AnimationCurve> curve;

  // Set for TIME_UPDATED events.
  base::Optional<base::TimeDelta> local_time;
};

class CC_ANIMATION_EXPORT AnimationEvents : public MutatorEvents {
 public:
  AnimationEvents();

  // MutatorEvents implementation.
  ~AnimationEvents() override;
  bool IsEmpty() const override;

  std::vector<AnimationEvent> events_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_EVENTS_H_
