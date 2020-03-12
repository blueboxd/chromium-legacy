// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_ANIMATION_PLAYBACK_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_ANIMATION_PLAYBACK_EVENT_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class AnimationPlaybackEventInit;

class AnimationPlaybackEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationPlaybackEvent* Create(
      const AtomicString& type,
      const AnimationPlaybackEventInit* initializer) {
    return MakeGarbageCollected<AnimationPlaybackEvent>(type, initializer);
  }

  AnimationPlaybackEvent(const AtomicString& type,
                         base::Optional<double> current_time,
                         base::Optional<double> timeline_time);
  AnimationPlaybackEvent(const AtomicString&,
                         const AnimationPlaybackEventInit*);
  ~AnimationPlaybackEvent() override;

  base::Optional<double> currentTime() const { return current_time_; }
  base::Optional<double> timelineTime() const { return timeline_time_; }
  // TODO(crbug.com/1060971): Remove |is_null| version.
  double currentTime(bool& is_null) const;   // DEPRECATED
  double timelineTime(bool& is_null) const;  // DEPRECATED

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) override;

 private:
  base::Optional<double> current_time_;
  base::Optional<double> timeline_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_ANIMATION_PLAYBACK_EVENT_H_
