/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/animation.h"

#include <limits>
#include <memory>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/events/animation_playback_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

unsigned NextSequenceNumber() {
  static unsigned next = 0;
  return ++next;
}

double ToMilliseconds(double seconds) {
  return seconds * 1000;
}

bool AreEqualOrNull(double a, double b) {
  // Null values are represented as NaNs, which have the property NaN != NaN.
  if (IsNull(a) && IsNull(b))
    return true;
  return a == b;
}

base::Optional<double> ValueOrUnresolved(double a) {
  base::Optional<double> value;
  if (!IsNull(a))
    value = a;
  return value;
}

void RecordCompositorAnimationFailureReasons(
    CompositorAnimations::FailureReasons failure_reasons) {
  // UMA_HISTOGRAM_ENUMERATION requires that the enum_max must be strictly
  // greater than the sample value. kFailureReasonCount doesn't include the
  // kNoFailure value but the histograms do so adding the +1 is necessary.
  // TODO(dcheng): Fix https://crbug.com/705169 so this isn't needed.
  constexpr uint32_t kFailureReasonEnumMax =
      CompositorAnimations::kFailureReasonCount + 1;

  if (failure_reasons == CompositorAnimations::kNoFailure) {
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.Animation.CompositedAnimationFailureReason",
        CompositorAnimations::kNoFailure, kFailureReasonEnumMax);
    return;
  }

  for (uint32_t i = 0; i < CompositorAnimations::kFailureReasonCount; i++) {
    unsigned val = 1 << i;
    if (failure_reasons & val) {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.Animation.CompositedAnimationFailureReason", i + 1,
          kFailureReasonEnumMax);
    }
  }
}
}  // namespace

Animation* Animation::Create(AnimationEffect* effect,
                             AnimationTimeline* timeline,
                             ExceptionState& exception_state) {
  DCHECK(timeline);
  if (!timeline->IsDocumentTimeline() && !timeline->IsScrollTimeline()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid timeline. Animation requires a "
                                      "DocumentTimeline or ScrollTimeline");
    return nullptr;
  }
  DCHECK(timeline->IsDocumentTimeline() || timeline->IsScrollTimeline());

  return MakeGarbageCollected<Animation>(
      timeline->GetDocument()->ContextDocument(), timeline, effect);
}

Animation* Animation::Create(ExecutionContext* execution_context,
                             AnimationEffect* effect,
                             ExceptionState& exception_state) {
  Document* document = To<Document>(execution_context);
  return Create(effect, &document->Timeline(), exception_state);
}

Animation* Animation::Create(ExecutionContext* execution_context,
                             AnimationEffect* effect,
                             AnimationTimeline* timeline,
                             ExceptionState& exception_state) {
  if (!timeline) {
    Animation* animation =
        MakeGarbageCollected<Animation>(execution_context, nullptr, effect);
    return animation;
  }

  return Create(effect, timeline, exception_state);
}

Animation::Animation(ExecutionContext* execution_context,
                     AnimationTimeline* timeline,
                     AnimationEffect* content)
    : ContextLifecycleObserver(execution_context),
      internal_play_state_(kIdle),
      animation_play_state_(kIdle),
      playback_rate_(1),
      start_time_(),
      hold_time_(),
      sequence_number_(NextSequenceNumber()),
      content_(content),
      timeline_(timeline),
      paused_(false),
      is_paused_for_testing_(false),
      is_composited_animation_disabled_for_testing_(false),
      pending_pause_(false),
      pending_play_(false),
      outdated_(false),
      finished_(true),
      compositor_state_(nullptr),
      compositor_pending_(false),
      compositor_group_(0),
      current_time_pending_(false),
      state_is_being_updated_(false),
      effect_suppressed_(false) {
  if (content_) {
    if (content_->GetAnimation()) {
      content_->GetAnimation()->cancel();
      content_->GetAnimation()->setEffect(nullptr);
    }
    content_->Attach(this);
  }
  document_ =
      timeline_ ? timeline_->GetDocument() : To<Document>(execution_context);
  DCHECK(document_);

  TickingTimeline().AnimationAttached(this);
  if (timeline_ && timeline_->IsScrollTimeline())
    timeline_->AnimationAttached(this);
  AttachCompositorTimeline();
  probe::DidCreateAnimation(document_, sequence_number_);
}

Animation::~Animation() {
  // Verify that compositor_animation_ has been disposed of.
  DCHECK(!compositor_animation_);
}

void Animation::Dispose() {
  if (timeline_ && timeline_->IsScrollTimeline())
    timeline_->AnimationDetached(this);
  DestroyCompositorAnimation();
  // If the DocumentTimeline and its Animation objects are
  // finalized by the same GC, we have to eagerly clear out
  // this Animation object's compositor animation registration.
  DCHECK(!compositor_animation_);
}

double Animation::EffectEnd() const {
  return content_ ? content_->SpecifiedTiming().EndTimeInternal() : 0;
}

bool Animation::Limited(double current_time) const {
  return (playback_rate_ < 0 && current_time <= 0) ||
         (playback_rate_ > 0 && current_time >= EffectEnd());
}

Document* Animation::GetDocument() {
  return document_;
}

double Animation::TimelineTime() const {
  if (timeline_)
    return timeline_->CurrentTime().value_or(NullValue());
  return NullValue();
}

DocumentTimeline& Animation::TickingTimeline() {
  // Active animations are tracked and ticked through the timeline attached to
  // the animation's document.
  // TODO(crbug.com/916117): Reconsider how animations are tracked and ticked.
  return document_->Timeline();
}

void Animation::setCurrentTime(double new_current_time,
                               bool is_null,
                               ExceptionState& exception_state) {
  // TODO(crbug.com/916117): Implement setting current time for scroll-linked
  // animations.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Scroll-linked WebAnimation currently does not support setting"
        " current time.");
    return;
  }

  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  // Step 1. of the procedure to silently set the current time of an
  // animation states that we abort if the new time is null.
  if (is_null) {
    // If the current time is resolved, then throw a TypeError.
    if (!IsNull(CurrentTimeInternal())) {
      exception_state.ThrowTypeError(
          "currentTime may not be changed from resolved to unresolved");
    }
    return;
  }

  if (PlayStateInternal() == kIdle)
    paused_ = true;

  current_time_pending_ = false;
  internal_play_state_ = kUnset;
  SetCurrentTimeInternal(new_current_time / 1000, kTimingUpdateOnDemand);

  if (CalculatePlayState() == kFinished)
    start_time_ = CalculateStartTime(new_current_time);
}

void Animation::SetCurrentTimeInternal(double new_current_time,
                                       TimingUpdateReason reason) {
  DCHECK(std::isfinite(new_current_time));

  bool outdated = false;
  bool is_limited = Limited(new_current_time);
  bool is_held = paused_ || !playback_rate_ || is_limited || !start_time_;
  if (is_held) {
    // We only need to update the animation if the seek changes the hold time.
    if (!hold_time_ || hold_time_ != new_current_time)
      outdated = true;
    hold_time_ = new_current_time;
    if (paused_ || !playback_rate_) {
      start_time_ = base::nullopt;
    } else if (is_limited && !start_time_ &&
               reason == kTimingUpdateForAnimationFrame) {
      start_time_ = CalculateStartTime(new_current_time);
    }
  } else {
    hold_time_ = base::nullopt;
    start_time_ = CalculateStartTime(new_current_time);
    finished_ = false;
    outdated = true;
  }

  if (outdated) {
    SetOutdated();
  }
}

// Update timing to reflect updated animation clock due to tick
void Animation::UpdateCurrentTimingState(TimingUpdateReason reason) {
  if (internal_play_state_ == kIdle || !timeline_ || !timeline_->IsActive())
    return;
  if (hold_time_) {
    double new_current_time = hold_time_.value();
    if (internal_play_state_ == kFinished && start_time_ && timeline_) {
      // Add hystersis due to floating point error accumulation
      if (!Limited(CalculateCurrentTime() + 0.001 * playback_rate_)) {
        // The current time became unlimited, eg. due to a backwards
        // seek of the timeline.
        new_current_time = CalculateCurrentTime();
      } else if (!Limited(hold_time_.value())) {
        // The hold time became unlimited, eg. due to the effect
        // becoming longer.
        new_current_time =
            clampTo<double>(CalculateCurrentTime(), 0, EffectEnd());
      }
    }
    SetCurrentTimeInternal(new_current_time, reason);
  } else if (Limited(CalculateCurrentTime())) {
    hold_time_ = playback_rate_ < 0 ? 0 : EffectEnd();
  }
}

double Animation::startTime(bool& is_null) const {
  base::Optional<double> result = startTime();
  is_null = !result;
  return result.value_or(0);
}

base::Optional<double> Animation::startTime() const {
  return start_time_ ? base::make_optional(start_time_.value() * 1000)
                     : base::nullopt;
}

double Animation::currentTime(bool& is_null) {
  double result = currentTime();
  is_null = std::isnan(result);
  return result;
}

// https://drafts.csswg.org/web-animations/#the-current-time-of-an-animation
double Animation::currentTime() {
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  // 1. If the animation’s hold time is resolved,
  //    The current time is the animation’s hold time.
  if (hold_time_.has_value())
    return ToMilliseconds(hold_time_.value());

  // 2.  If any of the following are true:
  //    * the animation has no associated timeline, or
  //    * the associated timeline is inactive, or
  //    * the animation’s start time is unresolved.
  // The current time is an unresolved time value.
  if (!timeline_ || !timeline_->IsActive() || PlayStateInternal() == kIdle ||
      !start_time_) {
    return NullValue();
  }

  // 3. Otherwise,
  // current time = (timeline time - start time) × playback rate
  base::Optional<double> timeline_time = timeline_->CurrentTimeSeconds();
  // TODO(crbug.com/916117): Handle NaN values for scroll linked animations.
  if (!timeline_time) {
    DCHECK(timeline_->IsScrollTimeline());
    return 0;
  }
  double current_time =
      (timeline_time.value() - start_time_.value()) * playback_rate_;
  return ToMilliseconds(current_time);
}

double Animation::CurrentTimeInternal() const {
  double result = hold_time_.value_or(CalculateCurrentTime());
#if DCHECK_IS_ON()
  // We can't enforce this check during Unset due to other assertions.
  if (internal_play_state_ != kUnset) {
    const_cast<Animation*>(this)->UpdateCurrentTimingState(
        kTimingUpdateOnDemand);
    double hold_or_current_time = hold_time_.value_or(CalculateCurrentTime());
    DCHECK(AreEqualOrNull(result, hold_or_current_time));
  }
#endif
  return result;
}

double Animation::UnlimitedCurrentTimeInternal() const {
#if DCHECK_IS_ON()
  CurrentTimeInternal();
#endif
  return PlayStateInternal() == kPaused || !start_time_
             ? CurrentTimeInternal()
             : CalculateCurrentTime();
}

bool Animation::PreCommit(
    int compositor_group,
    const PaintArtifactCompositor* paint_artifact_compositor,
    bool start_on_compositor) {
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand,
                                    kDoNotSetCompositorPending);

  bool soft_change =
      compositor_state_ &&
      (Paused() || compositor_state_->playback_rate != playback_rate_);
  bool hard_change =
      compositor_state_ && (compositor_state_->effect_changed ||
                            compositor_state_->start_time != start_time_ ||
                            !compositor_state_->start_time || !start_time_);

  // FIXME: softChange && !hardChange should generate a Pause/ThenStart,
  // not a Cancel, but we can't communicate these to the compositor yet.

  bool changed = soft_change || hard_change;
  bool should_cancel = (!Playing() && compositor_state_) || changed;
  bool should_start = Playing() && (!compositor_state_ || changed);

  if (start_on_compositor && should_cancel && should_start &&
      compositor_state_ && compositor_state_->pending_action == kStart) {
    // Restarting but still waiting for a start time.
    return false;
  }

  if (should_cancel) {
    CancelAnimationOnCompositor();
    compositor_state_ = nullptr;
  }

  DCHECK(!compositor_state_ || compositor_state_->start_time);

  if (!should_start) {
    current_time_pending_ = false;
  }

  if (should_start) {
    compositor_group_ = compositor_group;
    if (start_on_compositor) {
      CompositorAnimations::FailureReasons failure_reasons =
          CheckCanStartAnimationOnCompositor(paint_artifact_compositor);
      RecordCompositorAnimationFailureReasons(failure_reasons);

      if (failure_reasons == CompositorAnimations::kNoFailure) {
        CreateCompositorAnimation();
        StartAnimationOnCompositor(paint_artifact_compositor);
        compositor_state_ = std::make_unique<CompositorState>(*this);
      } else {
        CancelIncompatibleAnimationsOnCompositor();
      }
    }
  }

  return true;
}

void Animation::PostCommit(double timeline_time) {
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand,
                                    kDoNotSetCompositorPending);

  compositor_pending_ = false;

  if (!compositor_state_ || compositor_state_->pending_action == kNone)
    return;

  switch (compositor_state_->pending_action) {
    case kStart:
      if (compositor_state_->start_time) {
        DCHECK_EQ(start_time_.value(), compositor_state_->start_time.value());
        compositor_state_->pending_action = kNone;
      }
      break;
    case kPause:
    case kPauseThenStart:
      DCHECK(!start_time_);
      compositor_state_->pending_action = kNone;
      SetCurrentTimeInternal(
          (timeline_time - compositor_state_->start_time.value()) *
              playback_rate_,
          kTimingUpdateForAnimationFrame);
      current_time_pending_ = false;
      break;
    default:
      NOTREACHED();
  }
}

void Animation::NotifyCompositorStartTime(double timeline_time) {
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand,
                                    kDoNotSetCompositorPending);

  if (compositor_state_) {
    DCHECK_EQ(compositor_state_->pending_action, kStart);
    DCHECK(!compositor_state_->start_time);

    double initial_compositor_hold_time =
        compositor_state_->hold_time.value_or(NullValue());
    compositor_state_->pending_action = kNone;
    // TODO(crbug.com/791086): Determine whether this can ever be null.
    double start_time = timeline_time + CurrentTimeInternal() / -playback_rate_;
    compositor_state_->start_time =
        IsNull(start_time) ? base::nullopt : base::make_optional(start_time);

    if (start_time_ == timeline_time) {
      // The start time was set to the incoming compositor start time.
      // Unlikely, but possible.
      // FIXME: Depending on what changed above this might still be pending.
      // Maybe...
      current_time_pending_ = false;
      return;
    }

    if (start_time_ ||
        !AreEqualOrNull(CurrentTimeInternal(), initial_compositor_hold_time)) {
      // A new start time or current time was set while starting.
      SetCompositorPending(true);
      return;
    }
  }

  NotifyStartTime(timeline_time);
}

void Animation::NotifyStartTime(double timeline_time) {
  if (Playing()) {
    DCHECK(!start_time_);
    DCHECK(hold_time_.has_value());

    if (playback_rate_ == 0) {
      SetStartTimeInternal(timeline_time);
    } else {
      SetStartTimeInternal(timeline_time +
                           CurrentTimeInternal() / -playback_rate_);
    }

    // FIXME: This avoids marking this animation as outdated needlessly when a
    // start time is notified, but we should refactor how outdating works to
    // avoid this.
    ClearOutdated();
    current_time_pending_ = false;
  }
}

bool Animation::Affects(const Element& element,
                        const CSSProperty& property) const {
  if (!content_ || !content_->IsKeyframeEffect())
    return false;

  const KeyframeEffect* effect = ToKeyframeEffect(content_.Get());
  return (effect->target() == &element) &&
         effect->Affects(PropertyHandle(property));
}

base::Optional<double> Animation::CalculateStartTime(
    double current_time) const {
  base::Optional<double> start_time;
  if (timeline_) {
    base::Optional<double> timeline_time = timeline_->CurrentTimeSeconds();
    if (timeline_time)
      start_time = timeline_time.value() - current_time / playback_rate_;
    // TODO(crbug.com/916117): Handle NaN time for scroll-linked animations.
    DCHECK(start_time || timeline_->IsScrollTimeline());
  }
  return start_time;
}

double Animation::CalculateCurrentTime() const {
  if (!start_time_ || !timeline_ || !timeline_->IsActive())
    return NullValue();
  base::Optional<double> timeline_time = timeline_->CurrentTimeSeconds();
  // TODO(crbug.com/916117): Handle NaN time for scroll-linked animations.
  if (!timeline_time) {
    DCHECK(timeline_->IsScrollTimeline());
    return NullValue();
  }
  return (timeline_time.value() - start_time_.value()) * playback_rate_;
}

// https://drafts.csswg.org/web-animations/#setting-the-start-time-of-an-animation
void Animation::setStartTime(double start_time,
                             bool is_null,
                             ExceptionState& exception_state) {
  // TODO(crbug.com/916117): Implement setting start time for scroll-linked
  // animations.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Scroll-linked WebAnimation currently does not support setting start"
        " time.");
    return;
  }

  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  base::Optional<double> new_start_time;
  if (!is_null)
    new_start_time = start_time / 1000;

  // Setting the start time resolves the pending playback rate and cancels any
  // pending tasks regardless of whether setting to the current value.
  ResetPendingTasks();

  // Reevaluate the play state, as setting the start time can affect the
  // finished state.
  current_time_pending_ = false;
  internal_play_state_ = kUnset;

  SetStartTimeInternal(new_start_time);
}

void Animation::SetStartTimeInternal(base::Optional<double> new_start_time) {
  bool had_start_time = start_time_.has_value();
  double previous_current_time = CurrentTimeInternal();

  // Scroll-linked animations are initialized with the start time of
  // zero (i.e., scroll origin).
  // Changing scroll-linked animation start_time initialization is under
  // consideration here: https://github.com/w3c/csswg-drafts/issues/2075.
  start_time_ =
      (!timeline_ || timeline_->IsDocumentTimeline()) ? new_start_time : 0;

  // When we don't have an active timeline it is only possible to set either the
  // start time or the current time. Resetting the hold time clears current
  // time.
  if (!timeline_ && new_start_time.has_value())
    hold_time_ = base::nullopt;

  if (!new_start_time.has_value()) {
    hold_time_ = ValueOrUnresolved(previous_current_time);
    // Explicitly setting the start time to null pauses the animation. This
    // prevents the start time from simply being overridden when reevaluating
    // the play state.
    paused_ = true;
  } else if (hold_time_ && playback_rate_) {
    // If held, the start time would still be derived from the hold time.
    // Force a new, limited, current time.
    hold_time_ = base::nullopt;
    paused_ = false;
    double current_time = CalculateCurrentTime();
    if (playback_rate_ > 0 && current_time > EffectEnd()) {
      current_time = EffectEnd();
    } else if (playback_rate_ < 0 && current_time < 0) {
      current_time = 0;
    }
    SetCurrentTimeInternal(current_time, kTimingUpdateOnDemand);
  }
  UpdateCurrentTimingState(kTimingUpdateOnDemand);
  double new_current_time = CurrentTimeInternal();

  if (!AreEqualOrNull(previous_current_time, new_current_time)) {
    SetOutdated();
  } else if (!had_start_time && timeline_) {
    // Even though this animation is not outdated, time to effect change is
    // infinity until start time is set.
    ForceServiceOnNextFrame();
  }
}

void Animation::setEffect(AnimationEffect* new_effect) {
  if (content_ == new_effect)
    return;
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand,
                                    kSetCompositorPendingWithEffectChanged);

  double stored_current_time = CurrentTimeInternal();
  if (content_)
    content_->Detach();
  content_ = new_effect;
  if (new_effect) {
    // FIXME: This logic needs to be updated once groups are implemented
    if (new_effect->GetAnimation()) {
      new_effect->GetAnimation()->cancel();
      new_effect->GetAnimation()->setEffect(nullptr);
    }
    new_effect->Attach(this);
    SetOutdated();
  }
  if (!IsNull(stored_current_time))
    SetCurrentTimeInternal(stored_current_time, kTimingUpdateOnDemand);
}

const char* Animation::PlayStateString(AnimationPlayState play_state) {
  switch (play_state) {
    case kIdle:
      return "idle";
    case kPending:
      return "pending";
    case kRunning:
      return "running";
    case kPaused:
      return "paused";
    case kFinished:
      return "finished";
    default:
      NOTREACHED();
      return "";
  }
}

Animation::AnimationPlayState Animation::PlayStateInternal() const {
  DCHECK_NE(internal_play_state_, kUnset);
  return internal_play_state_;
}

Animation::AnimationPlayState Animation::CalculatePlayState() const {
  if (paused_ && !current_time_pending_)
    return kPaused;
  if (internal_play_state_ == kIdle)
    return kIdle;
  if (current_time_pending_ || (!start_time_ && playback_rate_ != 0))
    return kPending;
  if (Limited())
    return kFinished;
  return kRunning;
}

// https://drafts.csswg.org/web-animations/#play-states
Animation::AnimationPlayState Animation::CalculateAnimationPlayState() const {
  // 1. All of the following conditions are true:
  //    * The current time of animation is unresolved, and
  //    * animation does not have either a pending play task or a pending pause
  //      task,
  //    then idle.
  if (IsNull(CurrentTimeInternal()) && !pending())
    return kIdle;

  // 2. Either of the following conditions are true:
  //    * animation has a pending pause task, or
  //    * both the start time of animation is unresolved and it does not have a
  //      pending play task,
  //    then paused.
  // TODO(crbug.com/958433): Presently using a paused_ flag that tracks an
  // animation being in a paused state (including a pending pause). The above
  // rules do not yet work verbatim due to subtle timing discrepancies on when
  // start_time gets resolved.
  if (paused_)
    return kPaused;

  // 3.  For animation, current time is resolved and either of the following
  //     conditions are true:
  //     * animation’s effective playback rate > 0 and current time ≥ target
  //       effect end; or
  //     * animation’s effective playback rate < 0 and current time ≤ 0,
  //    then finished.
  if (Limited())
    return kFinished;

  // 4.  Otherwise
  return kRunning;
}

bool Animation::pending() const {
  return pending_pause_ || pending_play_;
}

// https://drafts.csswg.org/web-animations-1/#reset-an-animations-pending-tasks.
void Animation::ResetPendingTasks() {
  // We use an active playback rate instead of a pending playback rate to
  // sidestep complications of maintaining correct sequencing for updating
  // properties like current time and start time. Our implementation performs
  // calculations based on what will happen rather than waiting on a scheduled
  // task to commit the changes.
  // TODO(crbug.com/960944): Bring implementation into closer alignment with the
  // spec.
  active_playback_rate_.reset();
  pending_pause_ = false;
  pending_play_ = false;
}

void Animation::pause(ExceptionState& exception_state) {
  // TODO(crbug.com/916117): Implement pause for scroll-linked animations.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Scroll-linked WebAnimation currently does not support pause.");
    return;
  }
  if (paused_)
    return;

  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  double new_current_time = CurrentTimeInternal();
  if (CalculatePlayState() == kIdle || IsNull(new_current_time)) {
    if (playback_rate_ < 0 &&
        EffectEnd() == std::numeric_limits<double>::infinity()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot pause, Animation has infinite target effect end.");
      return;
    }
    new_current_time = playback_rate_ < 0 ? EffectEnd() : 0;
  }

  internal_play_state_ = kUnset;

  // We use two paused flags to indicate that the play state is paused, but that
  // the pause has not taken affect yet (pending). On the next async update,
  // paused will remain in affect, but the pending_pause_ flag will reset. The
  // pending pause can be interrupted via another change to the play state ahead
  // of the asynchronous update.
  // TODO(crbug.com/958433): We should not require the paused_ flag based on the
  // algorithm for determining play state in the spec. Currently, timing issues
  // prevent direct adoption of the algorithm in the spec.
  // (https://drafts.csswg.org/web-animations/#play-states).
  paused_ = true;
  pending_pause_ = true;

  current_time_pending_ = true;
  SetCurrentTimeInternal(new_current_time, kTimingUpdateOnDemand);
}

void Animation::Unpause() {
  if (!paused_)
    return;

  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  current_time_pending_ = true;
  UnpauseInternal();
}

void Animation::UnpauseInternal() {
  if (!paused_)
    return;
  paused_ = false;
  SetCurrentTimeInternal(CurrentTimeInternal(), kTimingUpdateOnDemand);
}

// https://drafts.csswg.org/web-animations/#playing-an-animation-section
void Animation::play(ExceptionState& exception_state) {
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  double current_time = this->CurrentTimeInternal();
  if (playback_rate_ < 0 && (current_time <= 0 || IsNull(current_time)) &&
      EffectEnd() == std::numeric_limits<double>::infinity()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot play reversed Animation with infinite target effect end.");
    return;
  }

  if (!Playing()) {
    start_time_ = base::nullopt;
  }

  if (PlayStateInternal() == kIdle) {
    hold_time_ = 0;
  }

  internal_play_state_ = kUnset;
  pending_play_ = true;
  finished_ = false;
  UnpauseInternal();

  if (playback_rate_ > 0 && (IsNull(current_time) || current_time < 0 ||
                             current_time >= EffectEnd())) {
    start_time_ = base::nullopt;
    SetCurrentTimeInternal(0, kTimingUpdateOnDemand);
  } else if (playback_rate_ < 0 && (IsNull(current_time) || current_time <= 0 ||
                                    current_time > EffectEnd())) {
    start_time_ = base::nullopt;
    SetCurrentTimeInternal(EffectEnd(), kTimingUpdateOnDemand);
  }
}

// https://drafts.csswg.org/web-animations/#reversing-an-animation-section
void Animation::reverse(ExceptionState& exception_state) {
  // TODO(crbug.com/916117): Implement reverse for scroll-linked animations.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Scroll-linked WebAnimation currently does not support reverse.");
    return;
  }
  if (!playback_rate_) {
    return;
  }

  if (!active_playback_rate_.has_value()) {
    active_playback_rate_ = playback_rate_;
  }

  double stored_playback_rate = playback_rate_;
  SetPlaybackRateInternal(-playback_rate_);
  play(exception_state);

  if (exception_state.HadException()) {
    SetPlaybackRateInternal(stored_playback_rate);
  }
}

// https://drafts.csswg.org/web-animations/#finishing-an-animation-section
void Animation::finish(ExceptionState& exception_state) {
  // Force resolution of PlayStateUpdateScope to enable immediate queuing of
  // the finished event.
  {
    PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

    if (!playback_rate_) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot finish Animation with a playbackRate of 0.");
      return;
    }
    if (playback_rate_ > 0 &&
        EffectEnd() == std::numeric_limits<double>::infinity()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot finish Animation with an infinite target effect end.");
      return;
    }

    // Avoid updating start time when already finished.
    if (CalculatePlayState() == kFinished)
      return;

    double new_current_time = playback_rate_ < 0 ? 0 : EffectEnd();
    SetCurrentTimeInternal(new_current_time, kTimingUpdateOnDemand);
    paused_ = false;
    current_time_pending_ = false;
    start_time_ = CalculateStartTime(new_current_time);
    internal_play_state_ = kFinished;
    ResetPendingTasks();
  }
  // Resolve finished event immediately.
  QueueFinishedEvent();
}

// https://drafts.csswg.org/web-animations/#setting-the-playback-rate-of-an-animation
void Animation::updatePlaybackRate(double playback_rate,
                                   ExceptionState& exception_state) {
  // TODO(crbug.com/916117): Implement updatePlaybackRate for scroll-linked
  // animations.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Scroll-linked WebAnimation currently does not support"
        " updatePlaybackRate.");
    return;
  }
  // The implementation differs from the spec; however, the end result is
  // consistent. Whereas Animation.playbackRate updates the playback rate
  // immediately, updatePlaybackRate is to take effect on the next async cycle.
  // From an implementation perspective, the difference lies in what gets
  // reported by the playbackRate getter ahead of the async update cycle, as the
  // Blink implementation guards against a discontinuity in current time even
  // when updating via the playbackRate setter.
  double stored_playback_rate = active_playback_rate_.value_or(playback_rate_);
  AnimationPlayState play_state = CalculateAnimationPlayState();
  if (play_state == kRunning)
    pending_play_ = true;

  setPlaybackRate(playback_rate);

  if (pending())
    active_playback_rate_ = stored_playback_rate;
}

ScriptPromise Animation::finished(ScriptState* script_state) {
  if (!finished_promise_) {
    finished_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state), this,
        AnimationPromise::kFinished);
    if (PlayStateInternal() == kFinished)
      finished_promise_->Resolve(this);
  }
  return finished_promise_->Promise(script_state->World());
}

ScriptPromise Animation::ready(ScriptState* script_state) {
  if (!ready_promise_) {
    ready_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state), this, AnimationPromise::kReady);
    if (PlayStateInternal() != kPending)
      ready_promise_->Resolve(this);
  }
  return ready_promise_->Promise(script_state->World());
}

const AtomicString& Animation::InterfaceName() const {
  return event_target_names::kAnimation;
}

ExecutionContext* Animation::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool Animation::HasPendingActivity() const {
  bool has_pending_promise =
      finished_promise_ &&
      finished_promise_->GetState() == ScriptPromisePropertyBase::kPending;

  return pending_finished_event_ || has_pending_promise ||
         (!finished_ && HasEventListeners(event_type_names::kFinish));
}

void Animation::ContextDestroyed(ExecutionContext*) {
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  finished_ = true;
  pending_finished_event_ = nullptr;
}

DispatchEventResult Animation::DispatchEventInternal(Event& event) {
  if (pending_finished_event_ == &event)
    pending_finished_event_ = nullptr;
  return EventTargetWithInlineData::DispatchEventInternal(event);
}

double Animation::playbackRate() const {
  // TODO(crbug.com/960944): Deviates from spec implementation, which instead
  // uses an 'effective playback rate' to be forward looking and 'playback rate'
  // for its current value.
  return active_playback_rate_.value_or(playback_rate_);
}

void Animation::setPlaybackRate(double playback_rate,
                                ExceptionState& exception_state) {
  // TODO(crbug.com/916117): Implement setting playback rate for scroll-linked
  // animations.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Scroll-linked WebAnimation currently does not support setting"
        " playback rate.");
    return;
  }
  active_playback_rate_.reset();
  if (playback_rate == playback_rate_)
    return;

  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  base::Optional<double> start_time_before = start_time_;
  SetPlaybackRateInternal(playback_rate);

  // Adds a UseCounter to check if setting playbackRate causes a compensatory
  // seek forcing a change in start_time_
  if (start_time_before && start_time_ != start_time_before &&
      internal_play_state_ != kFinished) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kAnimationSetPlaybackRateCompensatorySeek);
  }
}

void Animation::SetPlaybackRateInternal(double playback_rate) {
  DCHECK(std::isfinite(playback_rate));
  DCHECK_NE(playback_rate, playback_rate_);

  if (!Limited() && !Paused() && start_time_)
    current_time_pending_ = true;

  double stored_current_time = CurrentTimeInternal();
  if ((playback_rate_ < 0 && playback_rate >= 0) ||
      (playback_rate_ > 0 && playback_rate <= 0))
    finished_ = false;

  playback_rate_ = playback_rate;
  start_time_ = base::nullopt;
  if (!IsNull(stored_current_time))
    SetCurrentTimeInternal(stored_current_time, kTimingUpdateOnDemand);
}

void Animation::ClearOutdated() {
  if (!outdated_)
    return;
  outdated_ = false;
  if (timeline_)
    TickingTimeline().ClearOutdatedAnimation(this);
}

void Animation::SetOutdated() {
  if (outdated_)
    return;
  outdated_ = true;
  if (timeline_)
    TickingTimeline().SetOutdatedAnimation(this);
}

void Animation::ForceServiceOnNextFrame() {
  if (timeline_)
    TickingTimeline().Wake();
}

CompositorAnimations::FailureReasons
Animation::CheckCanStartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor) const {
  CompositorAnimations::FailureReasons reasons =
      CheckCanStartAnimationOnCompositorInternal();
  if (content_ && content_->IsKeyframeEffect()) {
    reasons |= ToKeyframeEffect(content_.Get())
                   ->CheckCanStartAnimationOnCompositor(
                       paint_artifact_compositor, playback_rate_);
  }
  return reasons;
}

CompositorAnimations::FailureReasons
Animation::CheckCanStartAnimationOnCompositorInternal() const {
  CompositorAnimations::FailureReasons reasons =
      CompositorAnimations::kNoFailure;

  if (is_composited_animation_disabled_for_testing_)
    reasons |= CompositorAnimations::kAcceleratedAnimationsDisabled;

  if (EffectSuppressed())
    reasons |= CompositorAnimations::kEffectSuppressedByDevtools;

  // An Animation with zero playback rate will produce no visual output, so
  // there is no reason to composite it.
  if (playback_rate_ == 0)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An infinite duration animation with a negative playback rate is essentially
  // a static value, so there is no reason to composite it.
  if (std::isinf(EffectEnd()) && playback_rate_ < 0)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation without a timeline effectively isn't playing, so there is no
  // reason to composite it. Additionally, mutating the timeline playback rate
  // is a debug feature available via devtools; we don't support this on the
  // compositor currently and there is no reason to do so.
  if (!timeline_ || (timeline_->IsDocumentTimeline() &&
                     ToDocumentTimeline(timeline_)->PlaybackRate() != 1))
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation without an effect cannot produce a visual, so there is no
  // reason to composite it.
  if (!content_ || !content_->IsKeyframeEffect())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation that is not playing will not produce a visual, so there is no
  // reason to composite it.
  if (!Playing())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // TODO(crbug.com/916117): Support accelerated scroll linked animations.
  if (timeline_->IsScrollTimeline())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  return reasons;
}

void Animation::StartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  DCHECK_EQ(CheckCanStartAnimationOnCompositor(paint_artifact_compositor),
            CompositorAnimations::kNoFailure);
  DCHECK(timeline_->IsDocumentTimeline());

  bool reversed = playback_rate_ < 0;

  base::Optional<double> start_time = base::nullopt;
  double time_offset = 0;
  if (start_time_) {
    start_time =
        ToDocumentTimeline(timeline_)->ZeroTime().since_origin().InSecondsF() +
        start_time_.value();
    if (reversed)
      start_time = start_time.value() - (EffectEnd() / fabs(playback_rate_));
  } else {
    time_offset =
        reversed ? EffectEnd() - CurrentTimeInternal() : CurrentTimeInternal();
    time_offset = time_offset / fabs(playback_rate_);
  }

  DCHECK(!start_time || !IsNull(start_time.value()));
  DCHECK_NE(compositor_group_, 0);
  DCHECK(ToKeyframeEffect(content_.Get()));
  ToKeyframeEffect(content_.Get())
      ->StartAnimationOnCompositor(compositor_group_, start_time, time_offset,
                                   playback_rate_);
}

void Animation::SetCompositorPending(bool effect_changed) {
  // Cannot play an animation with a null timeline.
  // TODO(crbug.com/827626) Revisit once timelines are mutable as there will be
  // work to do if the timeline is reset.
  if (!timeline_)
    return;

  // FIXME: KeyframeEffect could notify this directly?
  if (!HasActiveAnimationsOnCompositor()) {
    DestroyCompositorAnimation();
    compositor_state_.reset();
  }
  if (effect_changed && compositor_state_) {
    compositor_state_->effect_changed = true;
  }
  if (compositor_pending_ || is_paused_for_testing_) {
    return;
  }
  // In general, we need to update the compositor-side if anything has changed
  // on the blink version of the animation. There is also an edge case; if
  // neither the compositor nor blink side have a start time we still have to
  // sync them. This can happen if the blink side animation was started, the
  // compositor side hadn't started on its side yet, and then the blink side
  // start time was cleared (e.g. by setting current time).
  if (!compositor_state_ || compositor_state_->effect_changed ||
      compositor_state_->playback_rate != playback_rate_ ||
      compositor_state_->start_time != start_time_ ||
      !compositor_state_->start_time || !start_time_) {
    compositor_pending_ = true;
    document_->GetPendingAnimations().Add(this);
  }
}

void Animation::CancelAnimationOnCompositor() {
  if (HasActiveAnimationsOnCompositor()) {
    ToKeyframeEffect(content_.Get())
        ->CancelAnimationOnCompositor(GetCompositorAnimation());
  }

  DestroyCompositorAnimation();
}

void Animation::RestartAnimationOnCompositor() {
  if (!HasActiveAnimationsOnCompositor())
    return;
  if (ToKeyframeEffect(content_.Get())
          ->CancelAnimationOnCompositor(GetCompositorAnimation()))
    SetCompositorPending(true);
}

void Animation::CancelIncompatibleAnimationsOnCompositor() {
  if (content_ && content_->IsKeyframeEffect())
    ToKeyframeEffect(content_.Get())
        ->CancelIncompatibleAnimationsOnCompositor();
}

bool Animation::HasActiveAnimationsOnCompositor() {
  if (!content_ || !content_->IsKeyframeEffect())
    return false;

  return ToKeyframeEffect(content_.Get())->HasActiveAnimationsOnCompositor();
}

bool Animation::Update(TimingUpdateReason reason) {
  if (!timeline_)
    return false;

  if (reason == kTimingUpdateForAnimationFrame) {
    // Pending tasks are committed when the animation is 'ready'. This can be
    // at the time of promise resolution or after a frame tick.  Whereas the
    // spec calls for creating scheduled tasks to commit pending changes, in the
    // Blink implementation, this is an acknowledgment that the changes have
    // taken affect.
    // TODO(crbug.com/960944): Consider restructuring implementation to more
    // closely align with the recommended algorithm in the spec.
    ResetPendingTasks();
  }

  PlayStateUpdateScope update_scope(*this, reason, kDoNotSetCompositorPending);

  ClearOutdated();
  bool idle = PlayStateInternal() == kIdle;

  if (content_) {
    double inherited_time =
        idle || !timeline_->CurrentTime() ? NullValue() : CurrentTimeInternal();

    // Special case for end-exclusivity when playing backwards.
    if (inherited_time == 0 && playback_rate_ < 0)
      inherited_time = -1;

    content_->UpdateInheritedTime(inherited_time, reason);
    // After updating the animation time if the animation is no longer current
    // blink will no longer composite the element (see
    // CompositingReasonFinder::RequiresCompositingFor*Animation). We cancel any
    // running compositor animation so that we don't try to animate the
    // non-existent element on the compositor.
    if (!content_->IsCurrent())
      CancelAnimationOnCompositor();
  }

  if ((idle || Limited()) && !finished_) {
    if (reason == kTimingUpdateForAnimationFrame && (idle || start_time_)) {
      if (idle) {
        const AtomicString& event_type = event_type_names::kCancel;
        if (GetExecutionContext() && HasEventListeners(event_type)) {
          double event_current_time = NullValue();
          // TODO(crbug.com/916117): Handle NaN values for scroll-linked
          // animations.
          pending_cancelled_event_ =
              MakeGarbageCollected<AnimationPlaybackEvent>(
                  event_type, event_current_time, TimelineTime());
          pending_cancelled_event_->SetTarget(this);
          pending_cancelled_event_->SetCurrentTarget(this);
          document_->EnqueueAnimationFrameEvent(pending_cancelled_event_);
        }
      } else {
        QueueFinishedEvent();
      }
      finished_ = true;
    }
  }
  DCHECK(!outdated_);
  return !finished_ || std::isfinite(TimeToEffectChange());
}

void Animation::QueueFinishedEvent() {
  const AtomicString& event_type = event_type_names::kFinish;
  if (GetExecutionContext() && HasEventListeners(event_type)) {
    double event_current_time = CurrentTimeInternal() * 1000;
    // TODO(crbug.com/916117): Handle NaN values for scroll-linked animations.
    pending_finished_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
        event_type, event_current_time, TimelineTime());
    pending_finished_event_->SetTarget(this);
    pending_finished_event_->SetCurrentTarget(this);
    document_->EnqueueAnimationFrameEvent(pending_finished_event_);
  }
}

void Animation::UpdateIfNecessary() {
  // Update is a no-op if there is no timeline_, and will not reset the outdated
  // state in this case.
  if (!timeline_)
    return;

  if (Outdated())
    Update(kTimingUpdateOnDemand);
  DCHECK(!Outdated());
}

void Animation::EffectInvalidated() {
  SetOutdated();
  // FIXME: Needs to consider groups when added.
  SetCompositorPending(true);
}

bool Animation::IsEventDispatchAllowed() const {
  return Paused() || start_time_;
}

double Animation::TimeToEffectChange() {
  DCHECK(!outdated_);
  if (!start_time_ || hold_time_)
    return std::numeric_limits<double>::infinity();

  if (!content_)
    return -CurrentTimeInternal() / playback_rate_;
  double result = playback_rate_ > 0
                      ? content_->TimeToForwardsEffectChange() / playback_rate_
                      : content_->TimeToReverseEffectChange() / -playback_rate_;

  return !HasActiveAnimationsOnCompositor() &&
                 content_->GetPhase() == Timing::kPhaseActive
             ? 0
             : result;
}

void Animation::cancel() {
  PlayStateUpdateScope update_scope(*this, kTimingUpdateOnDemand);

  if (PlayStateInternal() == kIdle)
    return;

  hold_time_ = base::nullopt;
  paused_ = false;
  internal_play_state_ = kIdle;
  start_time_ = base::nullopt;
  current_time_pending_ = false;
  ResetPendingTasks();
  ForceServiceOnNextFrame();
}

void Animation::BeginUpdatingState() {
  // Nested calls are not allowed!
  DCHECK(!state_is_being_updated_);
  state_is_being_updated_ = true;
}

void Animation::EndUpdatingState() {
  DCHECK(state_is_being_updated_);
  state_is_being_updated_ = false;
}

void Animation::CreateCompositorAnimation() {
  if (Platform::Current()->IsThreadedAnimationEnabled() &&
      !compositor_animation_) {
    compositor_animation_ = CompositorAnimationHolder::Create(this);
    DCHECK(compositor_animation_);
    AttachCompositorTimeline();
  }

  AttachCompositedLayers();
}

void Animation::DestroyCompositorAnimation() {
  DetachCompositedLayers();

  if (compositor_animation_) {
    DetachCompositorTimeline();
    compositor_animation_->Detach();
    compositor_animation_ = nullptr;
  }
}

void Animation::AttachCompositorTimeline() {
  if (compositor_animation_) {
    CompositorAnimationTimeline* timeline =
        timeline_ ? ToDocumentTimeline(timeline_)->CompositorTimeline()
                  : nullptr;
    if (timeline)
      timeline->AnimationAttached(*this);
  }
}

void Animation::DetachCompositorTimeline() {
  if (compositor_animation_) {
    CompositorAnimationTimeline* timeline =
        timeline_ ? ToDocumentTimeline(timeline_)->CompositorTimeline()
                  : nullptr;
    if (timeline)
      timeline->AnimationDestroyed(*this);
  }
}

void Animation::AttachCompositedLayers() {
  if (!compositor_animation_)
    return;

  DCHECK(content_);
  DCHECK(content_->IsKeyframeEffect());

  ToKeyframeEffect(content_.Get())->AttachCompositedLayers();
}

void Animation::DetachCompositedLayers() {
  if (compositor_animation_ &&
      compositor_animation_->GetAnimation()->IsElementAttached())
    compositor_animation_->GetAnimation()->DetachElement();
}

void Animation::NotifyAnimationStarted(double monotonic_time, int group) {
  document_->GetPendingAnimations().NotifyCompositorAnimationStarted(
      monotonic_time, group);
}

Animation::PlayStateUpdateScope::PlayStateUpdateScope(
    Animation& animation,
    TimingUpdateReason reason,
    CompositorPendingChange compositor_pending_change)
    : animation_(animation),
      initial_play_state_(animation_->PlayStateInternal()),
      compositor_pending_change_(compositor_pending_change) {
  DCHECK_NE(initial_play_state_, kUnset);
  animation_->BeginUpdatingState();
  animation_->UpdateCurrentTimingState(reason);
}

Animation::PlayStateUpdateScope::~PlayStateUpdateScope() {
  AnimationPlayState old_play_state = initial_play_state_;
  AnimationPlayState new_play_state = animation_->CalculatePlayState();

  // TODO(crbug.com/958433): Phase out internal_play_state_ in favor of the spec
  // compliant version. At present, both are needed as the web exposed play
  // state cannot simply be inferred from the internal play state.
  animation_->internal_play_state_ = new_play_state;
  animation_->animation_play_state_ = animation_->CalculateAnimationPlayState();
  if (old_play_state != new_play_state) {
    bool was_active = old_play_state == kPending || old_play_state == kRunning;
    bool is_active = new_play_state == kPending || new_play_state == kRunning;
    if (!was_active && is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          animation_, "data", inspector_animation_event::Data(*animation_));
    } else if (was_active && !is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          animation_, "endData",
          inspector_animation_state_event::Data(*animation_));
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          animation_, "data",
          inspector_animation_state_event::Data(*animation_));
    }
  }

  // Ordering is important, the ready promise should resolve/reject before
  // the finished promise.
  if (animation_->ready_promise_ && new_play_state != old_play_state) {
    if (new_play_state == kIdle) {
      if (animation_->ready_promise_->GetState() ==
          AnimationPromise::kPending) {
        animation_->RejectAndResetPromiseMaybeAsync(
            animation_->ready_promise_.Get());
      } else {
        animation_->ready_promise_->Reset();
      }
      animation_->ResetPendingTasks();
      animation_->ResolvePromiseMaybeAsync(animation_->ready_promise_.Get());
    } else if (old_play_state == kPending) {
      animation_->ResetPendingTasks();
      animation_->ResolvePromiseMaybeAsync(animation_->ready_promise_.Get());
    } else if (new_play_state == kPending) {
      DCHECK_NE(animation_->ready_promise_->GetState(),
                AnimationPromise::kPending);
      animation_->ready_promise_->Reset();
    }
  }

  if (animation_->finished_promise_ && new_play_state != old_play_state) {
    if (new_play_state == kIdle) {
      if (animation_->finished_promise_->GetState() ==
          AnimationPromise::kPending) {
        animation_->RejectAndResetPromiseMaybeAsync(
            animation_->finished_promise_.Get());
      } else {
        animation_->finished_promise_->Reset();
      }
    } else if (new_play_state == kFinished) {
      animation_->ResetPendingTasks();
      animation_->ResolvePromiseMaybeAsync(animation_->finished_promise_.Get());
    } else if (old_play_state == kFinished) {
      animation_->finished_promise_->Reset();
    }
  }

  if (old_play_state != new_play_state &&
      (old_play_state == kIdle || new_play_state == kIdle)) {
    animation_->SetOutdated();
  }

#if DCHECK_IS_ON()
  // Verify that current time is up to date.
  animation_->CurrentTimeInternal();
#endif

  switch (compositor_pending_change_) {
    case kSetCompositorPending:
      animation_->SetCompositorPending();
      break;
    case kSetCompositorPendingWithEffectChanged:
      animation_->SetCompositorPending(true);
      break;
    case kDoNotSetCompositorPending:
      break;
    default:
      NOTREACHED();
      break;
  }
  animation_->EndUpdatingState();

  if (old_play_state != new_play_state) {
    probe::AnimationPlayStateChanged(animation_->document_, animation_,
                                     old_play_state, new_play_state);
  }
}

void Animation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == event_type_names::kFinish)
    UseCounter::Count(GetExecutionContext(), WebFeature::kAnimationFinishEvent);
}

void Animation::PauseForTesting(double pause_time) {
  SetCurrentTimeInternal(pause_time, kTimingUpdateOnDemand);
  if (HasActiveAnimationsOnCompositor())
    ToKeyframeEffect(content_.Get())
        ->PauseAnimationForTestingOnCompositor(CurrentTimeInternal());
  is_paused_for_testing_ = true;
  pause();
}

void Animation::SetEffectSuppressed(bool suppressed) {
  effect_suppressed_ = suppressed;
  if (suppressed)
    CancelAnimationOnCompositor();
}

void Animation::DisableCompositedAnimationForTesting() {
  is_composited_animation_disabled_for_testing_ = true;
  CancelAnimationOnCompositor();
}

void Animation::InvalidateKeyframeEffect(const TreeScope& tree_scope) {
  if (!content_ || !content_->IsKeyframeEffect())
    return;

  Element* target = ToKeyframeEffect(content_.Get())->target();

  // TODO(alancutter): Remove dependency of this function on CSSAnimations.
  // This function makes the incorrect assumption that the animation uses
  // @keyframes for its effect model when it may instead be using JS provided
  // keyframes.
  if (target &&
      CSSAnimations::IsAffectedByKeyframesFromScope(*target, tree_scope)) {
    target->SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    style_change_reason::kStyleSheetChange));
  }
}

void Animation::ResolvePromiseMaybeAsync(AnimationPromise* promise) {
  if (ScriptForbiddenScope::IsScriptForbidden()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&AnimationPromise::Resolve<Animation*>,
                             WrapPersistent(promise), WrapPersistent(this)));
  } else {
    promise->Resolve(this);
  }
}

void Animation::RejectAndResetPromise(AnimationPromise* promise) {
  promise->Reject(
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  promise->Reset();
}

void Animation::RejectAndResetPromiseMaybeAsync(AnimationPromise* promise) {
  if (ScriptForbiddenScope::IsScriptForbidden()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&Animation::RejectAndResetPromise,
                             WrapPersistent(this), WrapPersistent(promise)));
  } else {
    RejectAndResetPromise(promise);
  }
}

void Animation::Trace(blink::Visitor* visitor) {
  visitor->Trace(content_);
  visitor->Trace(document_);
  visitor->Trace(timeline_);
  visitor->Trace(pending_finished_event_);
  visitor->Trace(pending_cancelled_event_);
  visitor->Trace(finished_promise_);
  visitor->Trace(ready_promise_);
  visitor->Trace(compositor_animation_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

Animation::CompositorAnimationHolder*
Animation::CompositorAnimationHolder::Create(Animation* animation) {
  return MakeGarbageCollected<CompositorAnimationHolder>(animation);
}

Animation::CompositorAnimationHolder::CompositorAnimationHolder(
    Animation* animation)
    : animation_(animation) {
  compositor_animation_ = CompositorAnimation::Create();
  compositor_animation_->SetAnimationDelegate(animation_);
}

void Animation::CompositorAnimationHolder::Dispose() {
  if (!animation_)
    return;
  animation_->Dispose();
  DCHECK(!animation_);
  DCHECK(!compositor_animation_);
}

void Animation::CompositorAnimationHolder::Detach() {
  DCHECK(compositor_animation_);
  compositor_animation_->SetAnimationDelegate(nullptr);
  animation_ = nullptr;
  compositor_animation_.reset();
}
}  // namespace blink
