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
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
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

double SecondsToMilliseconds(double seconds) {
  return seconds * 1000;
}

double MillisecondsToSeconds(double milliseconds) {
  return milliseconds / 1000;
}

double Max(base::Optional<double> a, double b) {
  if (a.has_value())
    return std::max(a.value(), b);
  return b;
}

double Min(base::Optional<double> a, double b) {
  if (a.has_value())
    return std::min(a.value(), b);
  return b;
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
  if (!IsA<DocumentTimeline>(timeline) && !timeline->IsScrollTimeline()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid timeline. Animation requires a "
                                      "DocumentTimeline or ScrollTimeline");
    return nullptr;
  }
  DCHECK(IsA<DocumentTimeline>(timeline) || timeline->IsScrollTimeline());

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
      reported_play_state_(kIdle),
      playback_rate_(1),
      start_time_(),
      hold_time_(),
      sequence_number_(NextSequenceNumber()),
      content_(content),
      timeline_(timeline),
      is_paused_for_testing_(false),
      is_composited_animation_disabled_for_testing_(false),
      pending_pause_(false),
      pending_play_(false),
      pending_finish_notification_(false),
      has_queued_microtask_(false),
      outdated_(false),
      finished_(true),
      compositor_state_(nullptr),
      compositor_pending_(false),
      compositor_group_(0),
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

  if (timeline_)
    timeline_->AnimationAttached(this);
  else
    document_->Timeline().AnimationAttached(this);

  AttachCompositorTimeline();
  probe::DidCreateAnimation(document_, sequence_number_);
}

Animation::~Animation() {
  // Verify that compositor_animation_ has been disposed of.
  DCHECK(!compositor_animation_);
}

void Animation::Dispose() {
  if (timeline_)
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

bool Animation::Limited(base::Optional<double> current_time) const {
  return (EffectivePlaybackRate() < 0 && current_time <= 0) ||
         (EffectivePlaybackRate() > 0 && current_time >= EffectEnd());
}

Document* Animation::GetDocument() {
  return document_;
}

base::Optional<double> Animation::TimelineTime() const {
  return timeline_ ? timeline_->CurrentTime() : base::nullopt;
}

// https://drafts.csswg.org/web-animations/#setting-the-current-time-of-an-animation.
void Animation::setCurrentTime(double new_current_time,
                               bool is_null,
                               ExceptionState& exception_state) {
  // TODO(crbug.com/924159): Update this after we add support for inactive
  // timelines and unresolved timeline.currentTime
  if (is_null) {
    // If the current time is resolved, then throw a TypeError.
    if (CurrentTimeInternal()) {
      exception_state.ThrowTypeError(
          "currentTime may not be changed from resolved to unresolved");
    }
    return;
  }

  SetCurrentTimeInternal(MillisecondsToSeconds(new_current_time));

  // Synchronously resolve pending pause task.
  if (pending_pause_) {
    hold_time_ = MillisecondsToSeconds(new_current_time);
    ApplyPendingPlaybackRate();
    start_time_ = base::nullopt;
    pending_pause_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  // Update the finished state.
  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kAsync);

  SetCompositorPending(/*effect_changed=*/false);

  // Notify of potential state change.
  NotifyProbe();
}

// https://drafts.csswg.org/web-animations/#setting-the-current-time-of-an-animation
// See steps for silently setting the current time. The preliminary step of
// handling an unresolved time are to be handled by the caller.
void Animation::SetCurrentTimeInternal(double new_current_time) {
  DCHECK(std::isfinite(new_current_time));

  base::Optional<double> previous_start_time = start_time_;
  base::Optional<double> previous_hold_time = hold_time_;

  // Update either the hold time or the start time.
  if (hold_time_ || !start_time_ || !timeline_ || !timeline_->IsActive() ||
      playback_rate_ == 0)
    hold_time_ = new_current_time;
  else
    start_time_ = CalculateStartTime(new_current_time);

  // Preserve invariant that we can only set a start time or a hold time in the
  // absence of an active timeline.
  if (!timeline_ || !timeline_->IsActive())
    start_time_ = base::nullopt;

  // Reset the previous current time.
  previous_current_time_ = base::nullopt;

  if (previous_start_time != start_time_ || previous_hold_time != hold_time_)
    SetOutdated();
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
  // 1. If the animation’s hold time is resolved,
  //    The current time is the animation’s hold time.
  if (hold_time_.has_value())
    return SecondsToMilliseconds(hold_time_.value());

  // 2.  If any of the following are true:
  //    * the animation has no associated timeline, or
  //    * the associated timeline is inactive, or
  //    * the animation’s start time is unresolved.
  // The current time is an unresolved time value.
  if (!timeline_ || !timeline_->IsActive() || !start_time_) {
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
  return SecondsToMilliseconds(current_time);
}

base::Optional<double> Animation::CurrentTimeInternal() const {
  return hold_time_ ? hold_time_ : CalculateCurrentTime();
}

base::Optional<double> Animation::UnlimitedCurrentTime() const {
  return CalculateAnimationPlayState() == kPaused || !start_time_
             ? CurrentTimeInternal()
             : CalculateCurrentTime();
}

bool Animation::PreCommit(
    int compositor_group,
    const PaintArtifactCompositor* paint_artifact_compositor,
    bool start_on_compositor) {
  bool soft_change =
      compositor_state_ &&
      (Paused() || compositor_state_->playback_rate != EffectivePlaybackRate());
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
  compositor_pending_ = false;

  if (!compositor_state_ || compositor_state_->pending_action == kNone)
    return;

  DCHECK_EQ(kStart, compositor_state_->pending_action);
  if (compositor_state_->start_time) {
    DCHECK_EQ(start_time_.value(), compositor_state_->start_time.value());
    compositor_state_->pending_action = kNone;
  }
}

void Animation::NotifyReady(double ready_time) {
  // Complete the pending updates prior to updating the compositor state in
  // order to ensure a correct start time for the compositor state without the
  // need to duplicate the calculations.
  if (pending_play_)
    CommitPendingPlay(ready_time);
  else if (pending_pause_)
    CommitPendingPause(ready_time);

  if (compositor_state_ && compositor_state_->pending_action == kStart) {
    DCHECK(!compositor_state_->start_time);
    compositor_state_->pending_action = kNone;
    compositor_state_->start_time = start_time_;
  }

  // Notify of change to play state.
  NotifyProbe();
}

// Microtask for playing an animation.
// Refer to Step 8.3 'pending play task' in
// https://drafts.csswg.org/web-animations/#playing-an-animation-section.
void Animation::CommitPendingPlay(double ready_time) {
  DCHECK(!IsNull(ready_time));
  DCHECK(start_time_ || hold_time_);
  DCHECK(pending_play_);
  pending_play_ = false;

  // Update hold and start time.
  if (hold_time_) {
    // A: If animation’s hold time is resolved,
    // A.1. Apply any pending playback rate on animation.
    // A.2. Let new start time be the result of evaluating:
    //        ready time - hold time / playback rate for animation.
    //      If the playback rate is zero, let new start time be simply ready
    //      time.
    // A.3. Set the start time of animation to new start time.
    // A.4. If animation’s playback rate is not 0, make animation’s hold time
    //      unresolved.
    ApplyPendingPlaybackRate();
    if (playback_rate_ == 0) {
      start_time_ = ready_time;
    } else {
      start_time_ = ready_time - hold_time_.value() / playback_rate_;
      hold_time_ = base::nullopt;
    }
  } else if (start_time_ && pending_playback_rate_) {
    // B: If animation’s start time is resolved and animation has a pending
    //    playback rate,
    // B.1. Let current time to match be the result of evaluating:
    //        (ready time - start time) × playback rate for animation.
    // B.2 Apply any pending playback rate on animation.
    // B.3 If animation’s playback rate is zero, let animation’s hold time be
    //     current time to match.
    // B.4 Let new start time be the result of evaluating:
    //       ready time - current time to match / playback rate for animation.
    //     If the playback rate is zero, let new start time be simply ready
    //     time.
    // B.5 Set the start time of animation to new start time.
    double current_time_to_match =
        (ready_time - start_time_.value()) * playback_rate_;
    ApplyPendingPlaybackRate();
    if (playback_rate_ == 0) {
      hold_time_ = current_time_to_match;
      start_time_ = ready_time;
    } else {
      start_time_ = ready_time - current_time_to_match / playback_rate_;
    }
  }

  // 8.4 Resolve animation’s current ready promise with animation.
  if (ready_promise_ &&
      ready_promise_->GetState() == AnimationPromise::kPending)
    ResolvePromiseMaybeAsync(ready_promise_.Get());

  // 8.5 Run the procedure to update an animation’s finished state for
  //     animation with the did seek flag set to false, and the synchronously
  //     notify flag set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
}

// Microtask for pausing an animation.
// Refer to step 7 'pending pause task' in
// https://drafts.csswg.org/web-animations-1/#pausing-an-animation-section
void Animation::CommitPendingPause(double ready_time) {
  DCHECK(pending_pause_);
  pending_pause_ = false;

  // 1. Let ready time be the time value of the timeline associated with
  //    animation at the moment when the user agent completed processing
  //    necessary to suspend playback of animation’s associated effect.
  // 2. If animation’s start time is resolved and its hold time is not resolved,
  //    let animation’s hold time be the result of evaluating
  //    (ready time - start time) × playback rate.
  if (start_time_ && !hold_time_)
    hold_time_ = (ready_time - start_time_.value()) * playback_rate_;

  // 3. Apply any pending playback rate on animation.
  // 4. Make animation’s start time unresolved.
  ApplyPendingPlaybackRate();
  start_time_ = base::nullopt;

  // 5. Resolve animation’s current ready promise with animation.
  if (ready_promise_ &&
      ready_promise_->GetState() == AnimationPromise::kPending)
    ResolvePromiseMaybeAsync(ready_promise_.Get());

  // 6. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous), and the synchronously
  //    notify flag set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
}

bool Animation::Affects(const Element& element,
                        const CSSProperty& property) const {
  const auto* effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!effect)
    return false;

  return (effect->EffectTarget() == &element) &&
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

base::Optional<double> Animation::CalculateCurrentTime() const {
  if (!start_time_ || !timeline_ || !timeline_->IsActive())
    return base::nullopt;
  base::Optional<double> timeline_time = timeline_->CurrentTimeSeconds();
  // TODO(crbug.com/916117): Handle NaN time for scroll-linked animations.
  if (!timeline_time) {
    DCHECK(timeline_->IsScrollTimeline());
    return base::nullopt;
  }
  return (timeline_time.value() - start_time_.value()) * playback_rate_;
}

// https://drafts.csswg.org/web-animations/#setting-the-start-time-of-an-animation
void Animation::setStartTime(double start_time_ms,
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

  bool had_start_time = start_time_.has_value();

  // 1. Let timeline time be the current time value of the timeline that
  //    animation is associated with. If there is no timeline associated with
  //    animation or the associated timeline is inactive, let the timeline time
  //    be unresolved.
  base::Optional<double> timeline_time = timeline_ && timeline_->IsActive()
                                             ? timeline_->CurrentTimeSeconds()
                                             : base::nullopt;

  // 2. If timeline time is unresolved and new start time is resolved, make
  //    animation’s hold time unresolved.
  // This preserves the invariant that when we don’t have an active timeline it
  // is only possible to set either the start time or the animation’s current
  // time.
  if (!timeline_time && !is_null)
    hold_time_ = base::nullopt;

  // 3. Let previous current time be animation’s current time.
  base::Optional<double> previous_current_time = CurrentTimeInternal();

  // 4. Apply any pending playback rate on animation.
  ApplyPendingPlaybackRate();

  // 5. Set animation’s start time to new start time.
  base::Optional<double> new_start_time;
  if (!is_null)
    new_start_time = MillisecondsToSeconds(start_time_ms);
  start_time_ = new_start_time;

  // 6. Update animation’s hold time based on the first matching condition from
  //    the following,
  // 6a If new start time is resolved,
  //      If animation’s playback rate is not zero, make animation’s hold time
  //      unresolved.
  // 6b Otherwise (new start time is unresolved),
  //      Set animation’s hold time to previous current time even if previous
  //      current time is unresolved.
  if (start_time_) {
    if (playback_rate_ != 0)
      hold_time_ = base::nullopt;
  } else {
    hold_time_ = previous_current_time;
  }

  // 7. If animation has a pending play task or a pending pause task, cancel
  //    that task and resolve animation’s current ready promise with animation.
  if (pending()) {
    pending_pause_ = false;
    pending_play_ = false;
    if (ready_promise_ &&
        ready_promise_->GetState() == AnimationPromise::kPending)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  // 8. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to true (discontinuous), and the
  //    synchronously notify flag set to false (async).
  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kAsync);

  // Update user agent.
  base::Optional<double> new_current_time = CurrentTimeInternal();
  if (previous_current_time != new_current_time) {
    SetOutdated();
  } else if (!had_start_time && start_time_) {
    // Even though this animation is not outdated, time to effect change is
    // infinity until start time is set.
    ForceServiceOnNextFrame();
  }
  SetCompositorPending(/*effect_changed=*/false);

  NotifyProbe();
}

// https://drafts.csswg.org/web-animations-1/#setting-the-associated-effect
void Animation::setEffect(AnimationEffect* new_effect) {
  // 1. Let old effect be the current associated effect of animation, if any.
  AnimationEffect* old_effect = content_;

  // 2. If new effect is the same object as old effect, abort this procedure.
  if (new_effect == old_effect)
    return;

  // 3. If animation has a pending pause task, reschedule that task to run as
  //    soon as animation is ready.
  // 4. If animation has a pending play task, reschedule that task to run as
  //    soon as animation is ready to play new effect.
  // No special action required for a reschedule. The pending_pause_ and
  // pending_play_ flags remain unchanged.

  // 5. If new effect is not null and if new effect is the associated effect of
  //    another previous animation, run the procedure to set the associated
  //    effect of an animation (this procedure) on previous animation passing
  //    null as new effect.
  if (new_effect && new_effect->GetAnimation())
    new_effect->GetAnimation()->setEffect(nullptr);

  // 6. Let the associated effect of the animation be the new effect.
  if (old_effect)
    old_effect->Detach();
  content_ = new_effect;
  if (new_effect)
    new_effect->Attach(this);
  SetOutdated();

  // 7. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous), and the synchronously
  //    notify flag set to false (async).
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  SetCompositorPending(/*effect_change=*/true);

  // Notify of a potential state change.
  NotifyProbe();
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

// https://drafts.csswg.org/web-animations/#play-states
Animation::AnimationPlayState Animation::CalculateAnimationPlayState() const {
  // 1. All of the following conditions are true:
  //    * The current time of animation is unresolved, and
  //    * animation does not have either a pending play task or a pending pause
  //      task,
  //    then idle.
  if (!CurrentTimeInternal() && !pending())
    return kIdle;

  // 2. Either of the following conditions are true:
  //    * animation has a pending pause task, or
  //    * both the start time of animation is unresolved and it does not have a
  //      pending play task,
  //    then paused.
  if (pending_pause_ || (!start_time_ && !pending_play_))
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
  // 1. If animation does not have a pending play task or a pending pause task,
  //    abort this procedure.
  if (!pending())
    return;

  // 2. If animation has a pending play task, cancel that task.
  // 3. If animation has a pending pause task, cancel that task.
  pending_play_ = false;
  pending_pause_ = false;

  // 4. Apply any pending playback rate on animation.
  ApplyPendingPlaybackRate();

  // 5. Reject animation’s current ready promise with a DOMException named
  //    "AbortError".
  // 6. Let animation’s current ready promise be the result of creating a new
  //    resolved Promise object with value animation in the relevant Realm of
  //    animation.
  if (ready_promise_)
    RejectAndResetPromiseMaybeAsync(ready_promise_.Get());
}

// ----------------------------------------------
// Pause methods.
// ----------------------------------------------

// https://drafts.csswg.org/web-animations/#pausing-an-animation-section
void Animation::pause(ExceptionState& exception_state) {
  // TODO(crbug.com/916117): Implement pause for scroll-linked animations.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Scroll-linked WebAnimation currently does not support pause.");
    return;
  }

  // 1. If animation has a pending pause task, abort these steps.
  // 2. If the play state of animation is paused, abort these steps.
  if (pending_pause_ || CalculateAnimationPlayState() == kPaused)
    return;

  // 3.  If the animation’s current time is unresolved, perform the steps
  //     according to the first matching condition from below:
  // 3a. If animation’s playback rate is ≥ 0,
  //       Let animation’s hold time be zero.
  // 3b. Otherwise,
  //       If associated effect end for animation is positive infinity, throw an
  //       "InvalidStateError" DOMException and abort these steps. Otherwise,
  //       let animation’s hold time be associated effect end.
  base::Optional<double> current_time = CurrentTimeInternal();
  if (!current_time) {
    if (playback_rate_ >= 0) {
      hold_time_ = 0;
    } else {
      if (EffectEnd() == std::numeric_limits<double>::infinity()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "Cannot play reversed Animation with infinite target effect end.");
        return;
      }
      hold_time_ = EffectEnd();
    }
  }

  // 4. Let has pending ready promise be a boolean flag that is initially false.
  // 5. If animation has a pending play task, cancel that task and let has
  //    pending ready promise be true.
  // 6. If has pending ready promise is false, set animation’s current ready
  //    promise to a new promise in the relevant Realm of animation.
  if (pending_play_)
    pending_play_ = false;
  else if (ready_promise_)
    ready_promise_->Reset();

  // 7. Schedule a task to be executed at the first possible moment after the
  //    user agent has performed any processing necessary to suspend the
  //    playback of animation’s associated effect, if any.
  pending_pause_ = true;
  pending_play_ = false;

  SetOutdated();
  SetCompositorPending(false);

  // 8. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false (continuous) , and thesynchronously
  //    notify flag set to false.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  NotifyProbe();
}

// ----------------------------------------------
// Play methods.
// ----------------------------------------------

// Refer to the unpause operation in the following spec:
// https://drafts.csswg.org/css-animations-1/#animation-play-state
void Animation::Unpause() {
  if (CalculateAnimationPlayState() != kPaused)
    return;
  PlayInternal(AutoRewind::kDisabled, ASSERT_NO_EXCEPTION);
}

// https://drafts.csswg.org/web-animations/#programming-interface.
void Animation::play(ExceptionState& exception_state) {
  // Begin or resume playback of the animation by running the procedure to
  // play an animation passing true as the value of the auto-rewind flag.
  PlayInternal(AutoRewind::kEnabled, exception_state);
}

// https://drafts.csswg.org/web-animations/#playing-an-animation-section.
void Animation::PlayInternal(AutoRewind auto_rewind,
                             ExceptionState& exception_state) {
  // 1. Let aborted pause be a boolean flag that is true if animation has a
  //    pending pause task, and false otherwise.
  // 2. Let has pending ready promise be a boolean flag that is initially false.
  bool aborted_pause = pending_pause_;
  bool has_pending_ready_promise = false;

  // 3. Perform the steps corresponding to the first matching condition from the
  //    following, if any:
  //
  // 3a If animation’s effective playback rate > 0, the auto-rewind flag is true
  //    and either animation’s:
  //      current time is unresolved, or
  //      current time < zero, or
  //      current time ≥ target effect end,
  //    Set animation’s hold time to zero.
  //
  // 3b If animation’s effective playback rate < 0, the auto-rewind flag is true
  //    and either animation’s:
  //      current time is unresolved, or
  //      current time ≤ zero, or
  //      current time > target effect end,
  //    If target effect end is positive infinity, throw an "InvalidStateError"
  //    DOMException and abort these steps. Otherwise, set animation’s hold time
  //    to target effect end.
  //
  // 3c If animation’s effective playback rate = 0 and animation’s current time
  //    is unresolved,
  //    Set animation’s hold time to zero.
  double effective_playback_rate = EffectivePlaybackRate();
  base::Optional<double> current_time = CurrentTimeInternal();
  // TODO(crbug.com/1012073): This should be able to be extracted into a
  // function in AnimationTimeline that each child class can override for their
  // own special behavior.
  base::Optional<double> initial_hold_time =
      (timeline_ && timeline_->IsScrollTimeline())
          ? timeline_->CurrentTimeSeconds()
          : 0;
  if (effective_playback_rate > 0 && auto_rewind == AutoRewind::kEnabled &&
      (!current_time || current_time < 0 || current_time >= EffectEnd())) {
    hold_time_ = initial_hold_time;
  } else if (effective_playback_rate < 0 &&
             auto_rewind == AutoRewind::kEnabled &&
             (!current_time || current_time <= 0 ||
              current_time > EffectEnd())) {
    if (EffectEnd() == std::numeric_limits<double>::infinity()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot play reversed Animation with infinite target effect end.");
      return;
    }
    hold_time_ = EffectEnd();
  } else if (effective_playback_rate == 0 && !current_time) {
    hold_time_ = initial_hold_time;
  }

  // 4. If animation has a pending play task or a pending pause task,
  //   4.1 Cancel that task.
  //   4.2 Set has pending ready promise to true.
  if (pending_play_ || pending_pause_) {
    pending_play_ = pending_pause_ = false;
    has_pending_ready_promise = true;
  }

  // 5. If the following three conditions are all satisfied:
  //      animation’s hold time is unresolved, and
  //      aborted pause is false, and
  //      animation does not have a pending playback rate,
  //    abort this procedure.
  //
  // TODO(crbug.com/916117): Remove temporary extra condition for
  // scroll timelines. This is needed because hold_time_ can be set using
  // timeline.currentTime, which currently can return null. This check is to
  // make sure we don't abort in this case.
  if ((!hold_time_ &&
       (timeline_ &&
        !timeline_->IsScrollTimeline() /* Temporary extra condition */)) &&
      !aborted_pause && !pending_playback_rate_)
    return;

  // 6. If animation’s hold time is resolved, let its start time be unresolved.
  if (hold_time_)
    start_time_ = base::nullopt;

  // 7. If has pending ready promise is false, let animation’s current ready
  //    promise be a new promise in the relevant Realm of animation.
  if (ready_promise_ && !has_pending_ready_promise)
    ready_promise_->Reset();

  // 8. Schedule a task to run as soon as animation is ready.
  pending_play_ = true;
  finished_ = false;
  SetOutdated();
  SetCompositorPending(/*effect_changed=*/false);

  // 9. Run the procedure to update an animation’s finished state for animation
  //    with the did seek flag set to false, and the synchronously notify flag
  //    set to false.
  // Boolean valued arguments replaced with enumerated values for clarity.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  // Notify change to pending play or finished state.
  NotifyProbe();
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

  // 1. If there is no timeline associated with animation, or the associated
  //    timeline is inactive throw an "InvalidStateError" DOMException and abort
  //    these steps.
  if (!timeline_ || !timeline_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot reverse an animation with no active timeline");
    return;
  }

  // 2. Let original pending playback rate be animation’s pending playback rate.
  // 3. Let animation’s pending playback rate be the additive inverse of its
  //    effective playback rate (i.e. -effective playback rate).
  base::Optional<double> original_pending_playback_rate =
      pending_playback_rate_;
  pending_playback_rate_ = -EffectivePlaybackRate();

  // Resolve precision issue at zero.
  if (pending_playback_rate_.value() == -0)
    pending_playback_rate_ = 0;

  // 4. Run the steps to play an animation for animation with the auto-rewind
  //    flag set to true.
  //    If the steps to play an animation throw an exception, set animation’s
  //    pending playback rate to original pending playback rate and propagate
  //    the exception.
  PlayInternal(AutoRewind::kEnabled, exception_state);
  if (exception_state.HadException())
    pending_playback_rate_ = original_pending_playback_rate;
}

// ----------------------------------------------
// Finish methods.
// ----------------------------------------------

// https://drafts.csswg.org/web-animations/#finishing-an-animation-section
void Animation::finish(ExceptionState& exception_state) {
  if (!EffectivePlaybackRate()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot finish Animation with a playbackRate of 0.");
    return;
  }
  if (EffectivePlaybackRate() > 0 &&
      EffectEnd() == std::numeric_limits<double>::infinity()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot finish Animation with an infinite target effect end.");
    return;
  }

  ApplyPendingPlaybackRate();

  double new_current_time = playback_rate_ < 0 ? 0 : EffectEnd();
  SetCurrentTimeInternal(new_current_time);

  if (!start_time_ && timeline_ && timeline_->IsActive())
    start_time_ = CalculateStartTime(new_current_time);

  if (pending_pause_ && start_time_) {
    hold_time_ = base::nullopt;
    pending_pause_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }
  if (pending_play_ && start_time_) {
    pending_play_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  SetOutdated();
  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kSync);

  // Notify of change to finished state.
  NotifyProbe();
}

void Animation::UpdateFinishedState(UpdateType update_type,
                                    NotificationType notification_type) {
  bool did_seek = update_type == UpdateType::kDiscontinuous;
  // 1. Calculate the unconstrained current time. The dependency on did_seek is
  // required to accommodate timelines that may change direction. Without this
  // distinction, a once-finished animation would remain finished even when its
  // timeline progresses in the opposite direction.
  base::Optional<double> unconstrained_current_time =
      did_seek ? CurrentTimeInternal() : CalculateCurrentTime();

  // 2. Conditionally update the hold time.
  if (unconstrained_current_time && start_time_ && !pending_play_ &&
      !pending_pause_) {
    // Can seek outside the bounds of the active effect. Set the hold time to
    // the unconstrained value of the current time in the even that this update
    // this the result of explicitly setting the current time and the new time
    // is out of bounds. An update due to a time tick should not snap the hold
    // value back to the boundary if previously set outside the normal effect
    // boundary. The value of previous current time is used to retain this
    // value.
    double playback_rate = EffectivePlaybackRate();
    if (playback_rate > 0 && unconstrained_current_time >= EffectEnd()) {
      hold_time_ = did_seek ? unconstrained_current_time
                            : Max(previous_current_time_, EffectEnd());
    } else if (playback_rate < 0 && unconstrained_current_time <= 0) {
      hold_time_ = did_seek ? unconstrained_current_time
                            : Min(previous_current_time_, 0);
      // Hack for resolving precision issue at zero.
      if (hold_time_.value() == -0)
        hold_time_ = 0;
    } else if (playback_rate != 0) {
      // Update start time and reset hold time.
      if (did_seek && hold_time_)
        start_time_ = CalculateStartTime(hold_time_.value());
      hold_time_ = base::nullopt;
    }
  }

  // 3. Set the previous current time.
  previous_current_time_ = CurrentTimeInternal();

  // 4. Set the current finished state.
  AnimationPlayState play_state = CalculateAnimationPlayState();
  if (play_state == kFinished) {
    // 5. Setup finished notification.
    if (notification_type == NotificationType::kSync)
      CommitFinishNotification();
    else
      ScheduleAsyncFinish();
  } else {
    // 6. If not finished but the current finished promise is already resolved,
    //    create a new promise.
    finished_ = pending_finish_notification_ = false;
    if (finished_promise_ &&
        finished_promise_->GetState() == AnimationPromise::kResolved) {
      finished_promise_->Reset();
    }
  }
}

void Animation::ScheduleAsyncFinish() {
  // Run a task to handle the finished promise and event as a microtask. With
  // the exception of an explicit call to Animation::finish, it is important to
  // apply these updates asynchronously as it is possible to enter the finished
  // state temporarily.
  pending_finish_notification_ = true;
  if (!has_queued_microtask_) {
    Microtask::EnqueueMicrotask(
        WTF::Bind(&Animation::AsyncFinishMicrotask, WrapWeakPersistent(this)));
    has_queued_microtask_ = true;
  }
}

void Animation::AsyncFinishMicrotask() {
  // Resolve the finished promise and queue the finished event only if the
  // animation is still in a pending finished state. It is possible that the
  // transition was only temporary.
  if (pending_finish_notification_) {
    // A pending play or pause must resolve before the finish promise.
    if (pending() && timeline_)
      NotifyReady(timeline_->CurrentTimeSeconds().value_or(0));
    CommitFinishNotification();
  }

  // This is a once callback and needs to be re-armed.
  has_queued_microtask_ = false;
}

// Refer to 'finished notification steps' in
// https://drafts.csswg.org/web-animations-1/#updating-the-finished-state
void Animation::CommitFinishNotification() {
  pending_finish_notification_ = false;

  // 1. If animation’s play state is not equal to finished, abort these steps.
  if (CalculateAnimationPlayState() != kFinished)
    return;

  // 2. Resolve animation’s current finished promise object with animation.
  if (finished_promise_ &&
      finished_promise_->GetState() == AnimationPromise::kPending) {
    ResolvePromiseMaybeAsync(finished_promise_.Get());
  }

  // 3. Create an AnimationPlaybackEvent, finishEvent.
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

  // 1. Let previous play state be animation’s play state.
  // 2. Let animation’s pending playback rate be new playback rate.
  AnimationPlayState play_state = CalculateAnimationPlayState();
  pending_playback_rate_ = playback_rate;

  // 3. Perform the steps corresponding to the first matching condition from
  //    below:
  //
  // 3a If animation has a pending play task or a pending pause task,
  //    Abort these steps.
  if (pending())
    return;

  switch (play_state) {
    // 3b If previous play state is idle or paused,
    //    Apply any pending playback rate on animation.
    case kIdle:
    case kPaused:
      ApplyPendingPlaybackRate();
      break;

    // 3c If previous play state is finished,
    //    3c.1 Let the unconstrained current time be the result of calculating
    //         the current time of animation substituting an unresolved time
    //          value for the hold time.
    //    3c.2 Let animation’s start time be the result of evaluating the
    //         following expression:
    //    timeline time - (unconstrained current time / pending playback rate)
    // Where timeline time is the current time value of the timeline associated
    // with animation.
    //    3c.3 If pending playback rate is zero, let animation’s start time be
    //         timeline time.
    //    3c.4 Apply any pending playback rate on animation.
    //    3c.5 Run the procedure to update an animation’s finished state for
    //         animation with the did seek flag set to false, and the
    //         synchronously notify flag set to false.
    case kFinished: {
      base::Optional<double> unconstrained_current_time =
          CalculateCurrentTime();
      base::Optional<double> timeline_time =
          timeline_ ? timeline_->CurrentTimeSeconds() : base::nullopt;
      if (playback_rate) {
        if (timeline_time) {
          start_time_ =
              (timeline_time && unconstrained_current_time)
                  ? ValueOrUnresolved((timeline_time.value() -
                                       unconstrained_current_time.value()) /
                                      playback_rate)
                  : base::nullopt;
        }
      } else {
        start_time_ = timeline_time;
      }
      ApplyPendingPlaybackRate();
      UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
      break;
    }

    // 3d Otherwise,
    // Run the procedure to play an animation for animation with the
    // auto-rewind flag set to false.
    case kRunning:
      PlayInternal(AutoRewind::kDisabled, exception_state);
      break;

    case kUnset:
    case kPending:
      NOTREACHED();
  }
}

ScriptPromise Animation::finished(ScriptState* script_state) {
  if (!finished_promise_) {
    finished_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state));
    // Defer resolving the finished promise if the finish notification task is
    // pending. The finished state could change before the next microtask
    // checkpoint.
    if (CalculateAnimationPlayState() == kFinished &&
        !pending_finish_notification_)
      finished_promise_->Resolve(this);
  }
  return finished_promise_->Promise(script_state->World());
}

ScriptPromise Animation::ready(ScriptState* script_state) {
  if (!ready_promise_) {
    ready_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state));
    if (!pending())
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
      finished_promise_->GetState() == AnimationPromise::kPending;

  return pending_finished_event_ || has_pending_promise ||
         (!finished_ && HasEventListeners(event_type_names::kFinish));
}

void Animation::ContextDestroyed(ExecutionContext*) {
  finished_ = true;
  pending_finished_event_ = nullptr;
}

DispatchEventResult Animation::DispatchEventInternal(Event& event) {
  if (pending_finished_event_ == &event)
    pending_finished_event_ = nullptr;
  return EventTargetWithInlineData::DispatchEventInternal(event);
}

double Animation::playbackRate() const {
  return playback_rate_;
}

double Animation::EffectivePlaybackRate() const {
  return pending_playback_rate_.value_or(playback_rate_);
}

void Animation::ApplyPendingPlaybackRate() {
  if (pending_playback_rate_) {
    playback_rate_ = pending_playback_rate_.value();
    pending_playback_rate_ = base::nullopt;
  }
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

  base::Optional<double> start_time_before = start_time_;

  // 1. Clear any pending playback rate on animation.
  // 2. Let previous time be the value of the current time of animation before
  //    changing the playback rate.
  // 3. Set the playback rate to new playback rate.
  // 4. If previous time is resolved, set the current time of animation to
  //    previous time
  pending_playback_rate_ = base::nullopt;
  double previous_current_time = currentTime();
  playback_rate_ = playback_rate;
  if (!IsNull(previous_current_time)) {
    setCurrentTime(previous_current_time, false, exception_state);
  }

  // Adds a UseCounter to check if setting playbackRate causes a compensatory
  // seek forcing a change in start_time_
  if (start_time_before && start_time_ != start_time_before &&
      CalculateAnimationPlayState() != kFinished) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kAnimationSetPlaybackRateCompensatorySeek);
  }

  SetCompositorPending(false);
}

void Animation::ClearOutdated() {
  if (!outdated_)
    return;
  outdated_ = false;
  if (timeline_)
    timeline_->ClearOutdatedAnimation(this);
}

void Animation::SetOutdated() {
  if (outdated_)
    return;
  outdated_ = true;
  if (timeline_)
    timeline_->SetOutdatedAnimation(this);
}

void Animation::ForceServiceOnNextFrame() {
  if (timeline_)
    timeline_->ScheduleServiceOnNextFrame();
}

CompositorAnimations::FailureReasons
Animation::CheckCanStartAnimationOnCompositor(
    const PaintArtifactCompositor* paint_artifact_compositor) const {
  CompositorAnimations::FailureReasons reasons =
      CheckCanStartAnimationOnCompositorInternal();

  if (auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get())) {
    reasons |= keyframe_effect->CheckCanStartAnimationOnCompositor(
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
  if (EffectivePlaybackRate() == 0)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  if (!CurrentTimeInternal())
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // Cannot composite an infinite duration animation with a negative playback
  // rate. TODO(crbug.com/1029167): Fix calculation of compositor timing to
  // enable compositing provided the iteration duration is finite. Having an
  // infinite number of iterations in the animation should not impede the
  // ability to composite the animation.
  if (std::isinf(EffectEnd()) && EffectivePlaybackRate() < 0)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation without a timeline effectively isn't playing, so there is no
  // reason to composite it. Additionally, mutating the timeline playback rate
  // is a debug feature available via devtools; we don't support this on the
  // compositor currently and there is no reason to do so.
  auto* document_timeline = DynamicTo<DocumentTimeline>(*timeline_);
  if (!document_timeline || document_timeline->PlaybackRate() != 1)
    reasons |= CompositorAnimations::kInvalidAnimationOrEffect;

  // An Animation without an effect cannot produce a visual, so there is no
  // reason to composite it.
  if (!IsA<KeyframeEffect>(content_.Get()))
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
  DCHECK(IsA<DocumentTimeline>(*timeline_));

  bool reversed = EffectivePlaybackRate() < 0;

  base::Optional<double> start_time = base::nullopt;
  double time_offset = 0;
  // Start the animation on the compositor with either a start time or time
  // offset. The start time is used for synchronous updates where the
  // compositor start time must be in precise alignment with the specified time
  // (e.g. after calling setStartTime). Asynchronous updates such as updating
  // the playback rate preserve current time even if the start time is set.
  // Asynchronous updates have an associated pending play or pending pause
  // task associated with them.
  if (start_time_ && !pending()) {
    start_time = To<DocumentTimeline>(*timeline_)
                     .ZeroTime()
                     .since_origin()
                     .InSecondsF() +
                 start_time_.value();
    if (reversed) {
      start_time =
          start_time.value() - (EffectEnd() / fabs(EffectivePlaybackRate()));
    }
  } else {
    base::Optional<double> current_time = CurrentTimeInternal();
    DCHECK(current_time);
    time_offset =
        reversed ? EffectEnd() - current_time.value() : current_time.value();
    time_offset = time_offset / fabs(EffectivePlaybackRate());
  }

  DCHECK(!start_time || !IsNull(start_time.value()));
  DCHECK_NE(compositor_group_, 0);
  DCHECK(To<KeyframeEffect>(content_.Get()));
  DCHECK(std::isfinite(time_offset));
  To<KeyframeEffect>(content_.Get())
      ->StartAnimationOnCompositor(compositor_group_, start_time,
                                   base::TimeDelta::FromSecondsD(time_offset),
                                   EffectivePlaybackRate());
}

// TODO(crbug.com/960944): Rename to SetPendingCommit. This method handles both
// composited and non-composited animations. The use of 'compositor' in the name
// is confusing.
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
  if (pending() || !compositor_state_ || compositor_state_->effect_changed ||
      compositor_state_->playback_rate != EffectivePlaybackRate() ||
      compositor_state_->start_time != start_time_ ||
      !compositor_state_->start_time || !start_time_) {
    compositor_pending_ = true;
    document_->GetPendingAnimations().Add(this);
  }
}

void Animation::CancelAnimationOnCompositor() {
  if (HasActiveAnimationsOnCompositor()) {
    To<KeyframeEffect>(content_.Get())
        ->CancelAnimationOnCompositor(GetCompositorAnimation());
  }

  DestroyCompositorAnimation();
}

void Animation::RestartAnimationOnCompositor() {
  if (!HasActiveAnimationsOnCompositor())
    return;
  if (To<KeyframeEffect>(content_.Get())
          ->CancelAnimationOnCompositor(GetCompositorAnimation()))
    SetCompositorPending(true);
}

void Animation::CancelIncompatibleAnimationsOnCompositor() {
  if (auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get()))
    keyframe_effect->CancelIncompatibleAnimationsOnCompositor();
}

bool Animation::HasActiveAnimationsOnCompositor() {
  auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!keyframe_effect)
    return false;

  return keyframe_effect->HasActiveAnimationsOnCompositor();
}

// Update current time of the animation. Refer to step 1 in:
// https://drafts.csswg.org/web-animations/#update-animations-and-send-events
bool Animation::Update(TimingUpdateReason reason) {
  // Due to the hierarchical nature of the timing model, updating the current
  // time of an animation also involves:
  //   * Running the update an animation’s finished state procedure.
  //   * Queueing animation events.
  if (!timeline_)
    return false;

  ClearOutdated();
  bool idle = CalculateAnimationPlayState() == kIdle;
  if (!idle)
    UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);

  if (content_) {
    base::Optional<double> inherited_time = idle || !timeline_->CurrentTime()
                                                ? base::nullopt
                                                : CurrentTimeInternal();

    // Special case for end-exclusivity when playing backwards.
    if (inherited_time == 0 && EffectivePlaybackRate() < 0)
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

  if (reason == kTimingUpdateForAnimationFrame) {
    if (idle || CalculateAnimationPlayState() == kFinished) {
      // TODO(crbug.com/1029348): Per spec, we should have a microtask
      // checkpoint right after the update cycle. Once this is fixed we should
      // no longer need to force a synchronous resolution here.
      AsyncFinishMicrotask();
      finished_ = true;
    }
  }

  DCHECK(!outdated_);
  NotifyProbe();

  return !finished_ || TimeToEffectChange();
}

void Animation::QueueFinishedEvent() {
  const AtomicString& event_type = event_type_names::kFinish;
  if (GetExecutionContext() && HasEventListeners(event_type)) {
    base::Optional<double> event_current_time = CurrentTimeInternal();
    if (event_current_time)
      event_current_time = SecondsToMilliseconds(event_current_time.value());
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
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
  // FIXME: Needs to consider groups when added.
  SetCompositorPending(true);
}

bool Animation::IsEventDispatchAllowed() const {
  return Paused() || start_time_;
}

base::Optional<AnimationTimeDelta> Animation::TimeToEffectChange() {
  DCHECK(!outdated_);
  if (!start_time_ || hold_time_ || !playback_rate_)
    return base::nullopt;

  if (!content_) {
    base::Optional<double> current_time = CurrentTimeInternal();
    DCHECK(current_time);
    return AnimationTimeDelta::FromSecondsD(-current_time.value() /
                                            playback_rate_);
  }

  double result =
      playback_rate_ > 0
          ? content_->TimeToForwardsEffectChange().InSecondsF() / playback_rate_
          : content_->TimeToReverseEffectChange().InSecondsF() /
                -playback_rate_;

  return !HasActiveAnimationsOnCompositor() &&
                 content_->GetPhase() == Timing::kPhaseActive
             ? AnimationTimeDelta()
             : AnimationTimeDelta::FromSecondsD(result);
}

void Animation::cancel() {
  AnimationPlayState initial_play_state = CalculateAnimationPlayState();
  if (initial_play_state != kIdle) {
    ResetPendingTasks();

    if (finished_promise_) {
      if (finished_promise_->GetState() == AnimationPromise::kPending)
        RejectAndResetPromiseMaybeAsync(finished_promise_.Get());
      else
        finished_promise_->Reset();
    }

    const AtomicString& event_type = event_type_names::kCancel;
    if (GetExecutionContext() && HasEventListeners(event_type)) {
      base::Optional<double> event_current_time = base::nullopt;
      // TODO(crbug.com/916117): Handle NaN values for scroll-linked
      // animations.
      pending_cancelled_event_ = MakeGarbageCollected<AnimationPlaybackEvent>(
          event_type, event_current_time, TimelineTime());
      pending_cancelled_event_->SetTarget(this);
      pending_cancelled_event_->SetCurrentTarget(this);
      document_->EnqueueAnimationFrameEvent(pending_cancelled_event_);
    }
  } else {
    // Quietly reset without rejecting promises.
    pending_playback_rate_ = base::nullopt;
    pending_pause_ = pending_play_ = false;
  }

  hold_time_ = base::nullopt;
  start_time_ = base::nullopt;

  // Apply changes synchronously.
  SetCompositorPending(/*effect_changed=*/false);
  SetOutdated();

  // Force dispatch of canceled event.
  ForceServiceOnNextFrame();

  // Notify of change to canceled state.
  NotifyProbe();
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
        timeline_ ? To<DocumentTimeline>(*timeline_).CompositorTimeline()
                  : nullptr;
    if (timeline)
      timeline->AnimationAttached(*this);
  }
}

void Animation::DetachCompositorTimeline() {
  if (compositor_animation_) {
    CompositorAnimationTimeline* timeline =
        timeline_ ? To<DocumentTimeline>(*timeline_).CompositorTimeline()
                  : nullptr;
    if (timeline)
      timeline->AnimationDestroyed(*this);
  }
}

void Animation::AttachCompositedLayers() {
  if (!compositor_animation_)
    return;

  DCHECK(content_);
  DCHECK(IsA<KeyframeEffect>(*content_));

  To<KeyframeEffect>(content_.Get())->AttachCompositedLayers();
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

void Animation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == event_type_names::kFinish)
    UseCounter::Count(GetExecutionContext(), WebFeature::kAnimationFinishEvent);
}

void Animation::PauseForTesting(double pause_time) {
  // Do not restart a canceled animation.
  if (CalculateAnimationPlayState() == kIdle)
    return;

  // Pause a running animation, or update the hold time of a previously paused
  // animation.
  SetCurrentTimeInternal(pause_time);
  if (HasActiveAnimationsOnCompositor()) {
    base::Optional<double> current_time = CurrentTimeInternal();
    DCHECK(current_time);
    To<KeyframeEffect>(content_.Get())
        ->PauseAnimationForTestingOnCompositor(
            base::TimeDelta::FromSecondsD(current_time.value()));
  }

  // Do not wait for animation ready to lock in the hold time. Otherwise,
  // the pause won't take effect until the next frame and the hold time will
  // potentially drift.
  is_paused_for_testing_ = true;
  pending_pause_ = false;
  pending_play_ = false;
  hold_time_ = pause_time;
  start_time_ = base::nullopt;
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
  auto* keyframe_effect = DynamicTo<KeyframeEffect>(content_.Get());
  if (!keyframe_effect)
    return;

  Element* target = keyframe_effect->EffectTarget();

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

void Animation::NotifyProbe() {
  AnimationPlayState old_play_state = reported_play_state_;
  AnimationPlayState new_play_state =
      pending() ? kPending : CalculateAnimationPlayState();

  if (old_play_state != new_play_state) {
    if (!pending()) {
      probe::AnimationPlayStateChanged(document_, this, old_play_state,
                                       new_play_state);
    }
    reported_play_state_ = new_play_state;

    bool was_active = old_play_state == kPending || old_play_state == kRunning;
    bool is_active = new_play_state == kPending || new_play_state == kRunning;

    if (!was_active && is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data", inspector_animation_event::Data(*this));
    } else if (was_active && !is_active) {
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "endData", inspector_animation_state_event::Data(*this));
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
          "blink.animations,devtools.timeline,benchmark,rail", "Animation",
          this, "data", inspector_animation_state_event::Data(*this));
    }
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
