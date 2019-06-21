// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_ANIMATION_EFFECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_ANIMATION_EFFECT_H_

#include "third_party/blink/renderer/core/animation/effect_timing.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class MODULES_EXPORT WorkletAnimationEffect : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  WorkletAnimationEffect(base::Optional<base::TimeDelta> local_time,
                         const Timing& timing);

  // This function is named getTiming() as opposed to getEffectTiming() because
  // that is how it is defined in worklet_animation_effect.idl
  EffectTiming* getTiming() const;
  // Because getTiming is already used above, SpecifiedTiming will be used to
  // return the specified Timing object given at initialization
  const Timing& SpecifiedTiming() { return specified_timing_; }

  void setLocalTime(double time_ms, bool is_null);
  double localTime(bool& is_null) const;
  base::Optional<base::TimeDelta> local_time() const;

 private:
  base::Optional<base::TimeDelta> local_time_;
  // We chose to not call this variable "timing_" to avoid confusion with the
  // above function call getTiming() which returns a pointer to an EffectTiming
  // object, as is defined in worklet_animation_effect.idl.
  const Timing specified_timing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_ANIMATION_EFFECT_H_
