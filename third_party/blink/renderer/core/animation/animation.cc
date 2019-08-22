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
      animation_play_state_(kIdle),
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
      pending_effect_(false),
      pending_finish_notification_(false),
      has_queued_microtask_(false),
      outdated_(false),
      finished_(false),
      compositor_state_(nullptr),
      compositor_pending_(false),
      compositor_group_(0),
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
  return (EffectivePlaybackRate() < 0 && current_time <= 0) ||
         (EffectivePlaybackRate() > 0 && current_time >= EffectEnd());
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

// https://drafts.csswg.org/web-animations/#setting-the-current-time-of-an-animation.
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

  if (is_null) {
    // If the current time is resolved, then throw a TypeError.
    if (!IsNull(CurrentTimeInternal())) {
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
}

void Animation::SetCurrentTimeInternal(double new_current_time) {
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

  SetOutdated();
  SetCompositorPending();
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
double Animation::currentTime() const {
  // 1. If the animation’s hold time is resolved,
  //    The current time is the animation’s hold time.
  if (hold_time_)
    return SecondsToMilliseconds(hold_time_.value());

  // 2.  If any of the following are true:
  //    * the animation has no associated timeline, or
  //    * the associated timeline is inactive, or
  //    * the animation’s start time is unresolved.
  // The current time is an unresolved time value.
  if (!timeline_ || !timeline_->IsActive() || !start_time_)
    return NullValue();

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

double Animation::CurrentTimeInternal() const {
  return hold_time_.value_or(CalculateCurrentTime());
}

double Animation::UnlimitedCurrentTimeInternal() const {
  AnimationPlayState play_state = CalculateAnimationPlayState();
  return play_state == kPaused || !start_time_ ? CurrentTimeInternal()
                                               : CalculateCurrentTime();
}

bool Animation::PreCommit(
    int compositor_group,
    const PaintArtifactCompositor* paint_artifact_compositor,
    bool start_on_compositor) {
  // Early exit to prevent canceling an animation that is paused for testing.
  if (is_paused_for_testing_)
    return true;

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

  // Apply updates that do not requiring a sync up on a start time. Play-pending
  // animations are updated only after receiving a start time notification.
  if (!should_start && pending())
    ApplyUpdates();

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

void Animation::NotifyCompositorStartTime(double timeline_time) {
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
      ApplyUpdates(timeline_time);
      return;
    }

    if (start_time_ ||
        !AreEqualOrNull(CurrentTimeInternal(), initial_compositor_hold_time)) {
      // A new start time or current time was set while starting.
      SetCompositorPending(true);
      return;
    }
  }
  ApplyUpdates(timeline_time);
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

  base::Optional<double> new_start_time;
  if (!is_null)
    new_start_time = MillisecondsToSeconds(start_time);

  SetStartTimeInternal(new_start_time);
  NotifyProbe();
}

void Animation::SetStartTimeInternal(base::Optional<double> new_start_time) {
  // Scroll-linked animations are initialized with the start time of
  // zero (i.e., scroll origin).
  // Changing scroll-linked animation start_time initialization is under
  // consideration here: https://github.com/w3c/csswg-drafts/issues/2075.
  if (timeline_ && !timeline_->IsDocumentTimeline())
    new_start_time = 0;

  double previous_current_time = CurrentTimeInternal();
  bool had_start_time = start_time_.has_value();

  // Update start and hold times.
  ApplyPendingPlaybackRate();
  start_time_ = new_start_time;
  if (start_time_) {
    if (playback_rate_ != 0)
      hold_time_ = base::nullopt;
  } else {
    hold_time_ = ValueOrUnresolved(previous_current_time);
  }

  // Cancel pending tasks and resolve ready promise.
  if (pending_pause_ || pending_play_) {
    pending_pause_ = pending_play_ = false;
    if (ready_promise_)
      ResolvePromiseMaybeAsync(ready_promise_.Get());
  }

  UpdateFinishedState(UpdateType::kDiscontinuous, NotificationType::kAsync);

  double new_current_time = CurrentTimeInternal();
  if (!AreEqualOrNull(previous_current_time, new_current_time)) {
    SetOutdated();
  } else if (!had_start_time && timeline_) {
    // Even though this animation is not outdated, time to effect change is
    // infinity until start time is set.
    ForceServiceOnNextFrame();
  }
}

// https://drafts.csswg.org/web-animations/#setting-the-target-effect
void Animation::setEffect(AnimationEffect* new_effect) {
  if (content_ == new_effect)
    return;

  pending_effect_ = true;
  if (content_)
    content_->Detach();
  content_ = new_effect;
  if (new_effect) {
    // FIXME: This logic needs to be updated once groups are implemented
    if (new_effect->GetAnimation())
      new_effect->GetAnimation()->setEffect(nullptr);
    new_effect->Attach(this);
    SetOutdated();
  }

  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
  SetCompositorPending(true);
  NotifyProbe();
}

// ----------------------------------------------
// Play state methods.
// ----------------------------------------------

const char* Animation::PlayStateString(AnimationPlayState play_state) {
  switch (play_state) {
    case kIdle:
      return "idle";
    case kPending:  // TODO(crbug/958433): remove.
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
  double current_time = CurrentTimeInternal();
  if (IsNull(current_time) && !pending_pause_ && !pending_play_)
    return kIdle;

  // 2. Either of the following conditions are true:
  //    * animation has a pending pause task, or
  //    * both the start time of animation is unresolved and it does not have a
  //      pending play task,
  //    then paused.
  // By spec, paused wins out over finished. As a consequence of this ordering,
  // animations without active timelines are stuck in the paused state even if
  // explicitly calling finish on the animation.
  // TODO(crbug.com/967416): Address this edge case in the spec.
  if (pending_pause_ || (!start_time_ && !pending_play_))
    return kPaused;

  // 3.  For animation, current time is resolved and either of the following
  //     conditions are true:
  //     * animation’s effective playback rate > 0 and current time ≥ target
  //       effect end; or
  //     * animation’s effective playback rate < 0 and current time ≤ 0,
  //    then finished.
  if (!IsNull(current_time) && Limited(current_time))
    return kFinished;

  // 4.  Otherwise
  return kRunning;
}

// ----------------------------------------------
// Pending task management methods.
// ----------------------------------------------

bool Animation::pending() const {
  return pending_pause_ || pending_play_;
}

// https://drafts.csswg.org/web-animations-1/#reset-an-animations-pending-tasks.
bool Animation::ResetPendingTasks() {
  if (!pending())
    return false;

  pending_pause_ = false;
  pending_play_ = false;
  ApplyPendingPlaybackRate();

  if (ready_promise_)
    RejectAndResetPromiseMaybeAsync(ready_promise_.Get());

  return true;
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

  if (CalculateAnimationPlayState() == kPaused)
    return;

  double current_time = CurrentTimeInternal();

  // Jump to the start of the animation if the current time is unresolved.
  if (IsNull(current_time)) {
    if (playback_rate_ > 0) {
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

  // Cancel pending play task. If there was a pending play task, reuse it's
  // pending ready promise.
  if (pending_play_)
    pending_play_ = false;
  else if (ready_promise_)
    ready_promise_->Reset();

  // Schedule suspension of the animation.
  pending_pause_ = true;
  SetOutdated();
  SetCompositorPending(false);
  NotifyProbe();

  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
}

void Animation::CommitPendingPause(double ready_time) {
  DCHECK(pending_pause_);
  pending_pause_ = false;
  if (start_time_ && !hold_time_)
    hold_time_ = (ready_time - start_time_.value()) * playback_rate_;

  ApplyPendingPlaybackRate();
  start_time_ = base::nullopt;

  // Resolve pending ready promise.
  if (ready_promise_)
    ResolvePromiseMaybeAsync(ready_promise_.Get());

  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
}

// ----------------------------------------------
// Play methods.
// ----------------------------------------------

// Used with CSS animations.
void Animation::Unpause() {
  if (CalculateAnimationPlayState() != kPaused)
    return;
  play();
}

// https://drafts.csswg.org/web-animations/#playing-an-animation-section
void Animation::play(ExceptionState& exception_state) {
  PlayInternal(AutoRewind::kEnabled, exception_state);
}

void Animation::PlayInternal(AutoRewind auto_rewind,
                             ExceptionState& exception_state) {
  // Force an update of the animation state if idle. Handles the special case
  // of restarting an animation immediately after canceling.
  // TODO(crbug.com/960944): There are still a few unresolved bugs related to
  // event propagation and timing immediately after canceling an animation.
  // Revisit this forced updated after resolving these issues.
  if (CalculateAnimationPlayState() == kIdle) {
    SetOutdated();
    Update(kTimingUpdateOnDemand);
  }

  bool aborted_pause = pending_pause_;
  bool has_pending_ready_promise = false;
  double effective_playback_rate = EffectivePlaybackRate();
  double current_time = CurrentTimeInternal();

  // Update hold time.
  if (effective_playback_rate > 0 && auto_rewind == AutoRewind::kEnabled &&
      (IsNull(current_time) || current_time < 0 ||
       current_time >= EffectEnd())) {
    hold_time_ = 0;
  } else if (effective_playback_rate < 0 &&
             auto_rewind == AutoRewind::kEnabled &&
             (IsNull(current_time) || current_time <= 0 ||
              current_time > EffectEnd())) {
    if (EffectEnd() == std::numeric_limits<double>::infinity()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot play reversed Animation with infinite target effect end.");
      return;
    }
    hold_time_ = EffectEnd();
  } else if (effective_playback_rate == 0 && IsNull(current_time)) {
    hold_time_ = 0;
  }

  // Cancel pending play and pause tasks.
  if (pending_play_ || pending_pause_) {
    pending_play_ = pending_pause_ = false;
    has_pending_ready_promise = true;
  }

  if (!hold_time_ && !aborted_pause && !pending_playback_rate_)
    return;

  if (hold_time_)
    start_time_ = base::nullopt;

  // Reuse the pending promise. In the absence of a pending promise, create a
  // new one.
  if (!has_pending_ready_promise && ready_promise_)
    ready_promise_->Reset();

  // Schedule starting the animation.
  pending_play_ = true;
  SetOutdated();
  SetCompositorPending(false);
  NotifyProbe();

  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
}

void Animation::CommitPendingPlay(double ready_time) {
  DCHECK(start_time_ || hold_time_);
  DCHECK(pending_play_);
  pending_play_ = false;

  // Update hold and start time.
  if (timeline_ && timeline_->IsScrollTimeline()) {
    // Special handling for scroll timelines.  The start time is always zero
    // when the animation is playing. This forces the current time to match the
    // timeline time. TODO(crbug.com/916117): Resolve in spec.
    start_time_ = 0;
    ApplyPendingPlaybackRate();
    if (playback_rate_ != 0)
      hold_time_ = base::nullopt;
  } else if (hold_time_) {
    ApplyPendingPlaybackRate();
    if (playback_rate_ != 0) {
      start_time_ = ready_time - hold_time_.value() / playback_rate_;
      hold_time_ = base::nullopt;
    } else {
      start_time_ = ready_time;
    }
  } else if (start_time_ && pending_playback_rate_) {
    double current_time_to_match =
        (ready_time - start_time_.value()) * playback_rate_;
    ApplyPendingPlaybackRate();
    if (playback_rate_ == 0)
      hold_time_ = start_time_ = current_time_to_match;
    else
      start_time_ = ready_time - current_time_to_match / playback_rate_;
  }

  // Resolve pending ready promise.
  if (ready_promise_)
    ResolvePromiseMaybeAsync(ready_promise_.Get());

  // Update finished state.
  UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
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

  // Verify that the animation is associated with an active timeline.
  if (!timeline_ || !timeline_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot reverse an animation with no active timeline");
    return;
  }

  if (!playback_rate_)
    return;

  base::Optional<double> original_pending_playback_rate =
      pending_playback_rate_;
  pending_playback_rate_ = -EffectivePlaybackRate();

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
}

void Animation::UpdateFinishedState(UpdateType update_type,
                                    NotificationType notification_type) {
  bool did_seek = update_type == UpdateType::kDiscontinuous;
  // 1. Calculate the unconstrained current time. The dependency on did_seek is
  // required to accommodate timelines that may change direction. Without this
  // distinction, a once-finished animation would remain finished even when its
  // timeline progresses in the opposite direction.
  double unconstrained_current_time =
      did_seek ? CurrentTimeInternal() : CalculateCurrentTime();

  // 2. Conditionally update the hold time.
  if (!IsNull(unconstrained_current_time) && start_time_ && !pending_play_ &&
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
  previous_current_time_ = ValueOrUnresolved(CurrentTimeInternal());

  // 4. Set the current finished state.
  AnimationPlayState play_state = CalculateAnimationPlayState();

  if (play_state == kFinished) {
    if (!finished_) {
      // 5. Setup finished notification.
      pending_finish_notification_ = true;
      if (notification_type == NotificationType::kSync) {
        ApplyUpdates();
      } else {
        ScheduleAsyncFinish();
      }
    }
  } else {
    // 6. If not finished but the current finished promise is already resolved,
    //    create a new promise.
    pending_finish_notification_ = finished_ = false;
    if (finished_promise_ &&
        finished_promise_->GetState() == AnimationPromise::kResolved) {
      finished_promise_->Reset();
    }
  }
}

void Animation::CommitFinishNotification() {
  DCHECK(pending_finish_notification_);
  pending_finish_notification_ = false;
  if (finished_)
    return;
  finished_ = true;
  if (finished_promise_ &&
      finished_promise_->GetState() == AnimationPromise::kPending) {
    ResolvePromiseMaybeAsync(finished_promise_.Get());
  }
  QueueFinishedEvent();
}

ScriptPromise Animation::finished(ScriptState* script_state) {
  if (!finished_promise_) {
    finished_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state), this,
        AnimationPromise::kFinished);
    if (CalculateAnimationPlayState() == kFinished &&
        !pending_finish_notification_) {
      finished_promise_->Resolve(this);
    }
  }
  return finished_promise_->Promise(script_state->World());
}

ScriptPromise Animation::ready(ScriptState* script_state) {
  if (!ready_promise_) {
    ready_promise_ = MakeGarbageCollected<AnimationPromise>(
        ExecutionContext::From(script_state), this, AnimationPromise::kReady);
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
      finished_promise_->GetState() == ScriptPromisePropertyBase::kPending;

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

// --------------------------------------------------
// Speed control.
// --------------------------------------------------

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

  pending_playback_rate_ = base::nullopt;
  double previous_current_time = CurrentTimeInternal();
  playback_rate_ = playback_rate;
  if (!IsNull(previous_current_time)) {
    setCurrentTime(SecondsToMilliseconds(previous_current_time), false,
                   exception_state);
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

// https://drafts.csswg.org/web-animations/#seamlessly-updating-the-playback-rate-of-an-animation
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

  AnimationPlayState previous_play_state = CalculateAnimationPlayState();

  pending_playback_rate_ = playback_rate;
  if (pending_play_ || pending_pause_)
    return;

  switch (previous_play_state) {
    case kIdle:
    case kPaused:
      ApplyPendingPlaybackRate();
      break;

    case kFinished:
      start_time_ = (playback_rate == 0)
                        ? ValueOrUnresolved(TimelineTime())
                        : CalculateStartTime(CurrentTimeInternal());
      ApplyPendingPlaybackRate();
      UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
      break;

    default:
      PlayInternal(AutoRewind::kDisabled, exception_state);
  }
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

// -------------------------------------
// Updating the state of an animation
// -------------------------------------

void Animation::ScheduleAsyncFinish() {
  // Run a task to handle the finished promise and event as a microtask. With
  // the exception of an explicit call to Animation::finish, it is important to
  // apply these updates asynchronously as it is possible to enter the finished
  // state temporarily.
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
  if (pending_finish_notification_)
    ApplyUpdates();

  // This is a once callback and needs to be re-armed.
  has_queued_microtask_ = false;
}

void Animation::ApplyUpdates() {
  base::Optional<double> timeline_time;
  if (timeline_)
    timeline_time = timeline_->CurrentTimeSeconds();
  ApplyUpdates(timeline_time.value_or(NullValue()));
}

void Animation::ApplyUpdates(double ready_time) {
  // Applies all updates to an animation. The state of the animation may change
  // between the time that pending updates are triggered and when the updates
  // are applied. Thus, flags are used instead of directly queuing callbacks
  // to enable validation that the pending state change is still relevant.
  // Multiple updates may be applied with the caveat that the pending play and
  // pending pause are mutually exclusive.
  DCHECK(!(pending_play_ && pending_pause_));
  if (pending_play_)
    CommitPendingPlay(ready_time);
  else if (pending_pause_)
    CommitPendingPause(ready_time);

  ApplyPendingPlaybackRate();

  if (pending_finish_notification_)
    CommitFinishNotification();

  DCHECK(!pending_play_);
  DCHECK(!pending_pause_);
  DCHECK(!pending_finish_notification_);
  DCHECK(!pending_playback_rate_);

  NotifyProbe();
}

void Animation::NotifyProbe() {
  AnimationPlayState old_play_state = animation_play_state_;
  // 'Pending' is not a true play state for the animation. Dev tools is relying
  // on an extended play state.
  // TODO(crbug.com/958433): Fix dependency on pending state.
  AnimationPlayState new_play_state =
      pending() ? kPending : CalculateAnimationPlayState();

  if (old_play_state != new_play_state) {
    probe::AnimationPlayStateChanged(document_, this, old_play_state,
                                     new_play_state);
    animation_play_state_ = new_play_state;

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

// --------------------------------------------
// CompositorAnimations methods
// --------------------------------------------

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
      compositor_state_->playback_rate != EffectivePlaybackRate() ||
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
          ->CancelAnimationOnCompositor(GetCompositorAnimation())) {
    SetCompositorPending(true);
  }
}

void Animation::CancelIncompatibleAnimationsOnCompositor() {
  if (content_ && content_->IsKeyframeEffect()) {
    ToKeyframeEffect(content_.Get())
        ->CancelIncompatibleAnimationsOnCompositor();
  }
}

bool Animation::HasActiveAnimationsOnCompositor() {
  if (!content_ || !content_->IsKeyframeEffect())
    return false;

  return ToKeyframeEffect(content_.Get())->HasActiveAnimationsOnCompositor();
}

bool Animation::Update(TimingUpdateReason reason) {
  if (!timeline_)
    return false;

  ClearOutdated();
  bool idle = CalculateAnimationPlayState() == kIdle;

  if (content_) {
    double inherited_time =
        idle || !timeline_->CurrentTime() ? NullValue() : CurrentTimeInternal();

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

  bool finished = false;
  if (reason == kTimingUpdateForAnimationFrame) {
    UpdateFinishedState(UpdateType::kContinuous, NotificationType::kAsync);
    if (CalculateAnimationPlayState() == kFinished) {
      if (pending_finish_notification_)
        ApplyUpdates();
      finished = true;
    }
  }

  DCHECK(!outdated_);
  return !finished || TimeToEffectChange();
}

void Animation::QueueFinishedEvent() {
  const AtomicString& event_type = event_type_names::kFinish;
  if (GetExecutionContext() && HasEventListeners(event_type)) {
    double event_current_time = SecondsToMilliseconds(CurrentTimeInternal());
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

base::Optional<AnimationTimeDelta> Animation::TimeToEffectChange() {
  DCHECK(!outdated_);
  if (!start_time_ || hold_time_ || !playback_rate_)
    return base::nullopt;

  if (!content_) {
    return AnimationTimeDelta::FromSecondsD(-CurrentTimeInternal() /
                                            playback_rate_);
  }

  if (!HasActiveAnimationsOnCompositor() &&
      content_->GetPhase() == Timing::kPhaseActive) {
    return AnimationTimeDelta();
  }

  double result = playback_rate_ > 0
                      ? content_->TimeToForwardsEffectChange() / playback_rate_
                      : content_->TimeToReverseEffectChange() / -playback_rate_;

  if (!std::isfinite(result))
    return base::nullopt;

  return AnimationTimeDelta::FromSecondsD(result);
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
      double event_current_time = NullValue();
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

  pending_finish_notification_ = false;

  // Apply changes synchronously.
  SetCompositorPending(/*effect_changed=*/false);
  SetOutdated();
  ApplyUpdates();

  // Force dispatch of canceled event.
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

void Animation::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == event_type_names::kFinish)
    UseCounter::Count(GetExecutionContext(), WebFeature::kAnimationFinishEvent);
}

void Animation::PauseForTesting(double pause_time) {
  // Do not restart an animation if already canceled.
  if (CalculateAnimationPlayState() == kIdle)
    return;

  SetCurrentTimeInternal(pause_time);
  if (HasActiveAnimationsOnCompositor()) {
    ToKeyframeEffect(content_.Get())
        ->PauseAnimationForTestingOnCompositor(pause_time);
  }
  is_paused_for_testing_ = true;
  pause();
  // Do not wait for animation ready to lock in the hold time. Otherwise,
  // the pause won't take effect until the next frame and the hold time will
  // potentially drift.
  ApplyUpdates();
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

// ------------------------------------
// Promise resolution
// ------------------------------------

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
