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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/animation_effect_owner.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_client.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CompositorAnimation;
class Element;
class ExceptionState;
class PaintArtifactCompositor;
class TreeScope;

class CORE_EXPORT Animation final : public EventTargetWithInlineData,
                                    public ActiveScriptWrappable<Animation>,
                                    public ContextLifecycleObserver,
                                    public CompositorAnimationDelegate,
                                    public CompositorAnimationClient,
                                    public AnimationEffectOwner {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Animation);

 public:
  enum AnimationPlayState {
    kUnset,
    kIdle,
    kPending,
    kRunning,
    kPaused,
    kFinished
  };

  static Animation* Create(AnimationEffect*,
                           AnimationTimeline*,
                           ExceptionState& = ASSERT_NO_EXCEPTION);

  // Web Animations API IDL constructors.
  static Animation* Create(ExecutionContext*,
                           AnimationEffect*,
                           ExceptionState&);
  static Animation* Create(ExecutionContext*,
                           AnimationEffect*,
                           AnimationTimeline*,
                           ExceptionState&);

  Animation(ExecutionContext*, AnimationTimeline*, AnimationEffect*);
  ~Animation() override;
  void Dispose();

  // Returns whether the animation is finished.
  bool Update(TimingUpdateReason);

  // AnimationEffectOwner:
  void UpdateIfNecessary() override;
  void EffectInvalidated() override;
  bool IsEventDispatchAllowed() const override;
  Animation* GetAnimation() override { return this; }

  // timeToEffectChange returns:
  //  infinity  - if this animation is no longer in effect
  //  0         - if this animation requires an update on the next frame
  //  n         - if this animation requires an update after 'n' units of time
  double TimeToEffectChange();

  void cancel();

  double currentTime(bool& is_null);
  double currentTime();
  void setCurrentTime(double new_current_time,
                      bool is_null,
                      ExceptionState& = ASSERT_NO_EXCEPTION);

  double CurrentTimeInternal() const;
  double UnlimitedCurrentTimeInternal() const;

  void SetCurrentTimeInternal(double new_current_time,
                              TimingUpdateReason = kTimingUpdateOnDemand);
  bool Paused() const { return paused_ && !is_paused_for_testing_; }
  static const char* PlayStateString(AnimationPlayState);
  String playState() const { return PlayStateString(animation_play_state_); }
  AnimationPlayState PlayStateInternal() const;
  bool pending() const;

  void pause(ExceptionState& = ASSERT_NO_EXCEPTION);
  void play(ExceptionState& = ASSERT_NO_EXCEPTION);
  void reverse(ExceptionState& = ASSERT_NO_EXCEPTION);
  void finish(ExceptionState& = ASSERT_NO_EXCEPTION);
  void updatePlaybackRate(double playback_rate,
                          ExceptionState& = ASSERT_NO_EXCEPTION);

  ScriptPromise finished(ScriptState*);
  ScriptPromise ready(ScriptState*);

  bool Playing() const override {
    return !(PlayStateInternal() == kIdle || Limited() || paused_ ||
             is_paused_for_testing_);
  }
  bool Limited() const { return Limited(CurrentTimeInternal()); }
  bool FinishedInternal() const { return finished_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(finish, kFinish)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(cancel, kCancel)

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  bool HasPendingActivity() const final;
  void ContextDestroyed(ExecutionContext*) override;

  double playbackRate() const;
  void setPlaybackRate(double, ExceptionState& = ASSERT_NO_EXCEPTION);
  AnimationTimeline* timeline() { return timeline_; }
  Document* GetDocument();

  double startTime(bool& is_null) const;
  base::Optional<double> startTime() const;
  base::Optional<double> StartTimeInternal() const { return start_time_; }
  void setStartTime(double,
                    bool is_null,
                    ExceptionState& = ASSERT_NO_EXCEPTION);

  const AnimationEffect* effect() const { return content_.Get(); }
  AnimationEffect* effect() { return content_.Get(); }
  void setEffect(AnimationEffect*);

  void setId(const String& id) { id_ = id; }
  const String& id() const { return id_; }

  // Pausing via this method is not reflected in the value returned by
  // paused() and must never overlap with pausing via pause().
  void PauseForTesting(double pause_time);
  void DisableCompositedAnimationForTesting();

  // This should only be used for CSS
  void Unpause();

  void SetOutdated();
  bool Outdated() { return outdated_; }

  CompositorAnimations::FailureReasons CheckCanStartAnimationOnCompositor(
      const PaintArtifactCompositor* paint_artifact_compositor) const;
  void StartAnimationOnCompositor(
      const PaintArtifactCompositor* paint_artifact_compositor);
  void CancelAnimationOnCompositor();
  void RestartAnimationOnCompositor();
  void CancelIncompatibleAnimationsOnCompositor();
  bool HasActiveAnimationsOnCompositor();
  void SetCompositorPending(bool effect_changed = false);
  void NotifyCompositorStartTime(double timeline_time);
  void NotifyStartTime(double timeline_time);
  // CompositorAnimationClient implementation.
  CompositorAnimation* GetCompositorAnimation() const override {
    return compositor_animation_ ? compositor_animation_->GetAnimation()
                                 : nullptr;
  }

  bool Affects(const Element&, const CSSProperty&) const;

  // Returns whether we should continue with the commit for this animation or
  // wait until next commit.
  bool PreCommit(int compositor_group,
                 const PaintArtifactCompositor*,
                 bool start_on_compositor);
  void PostCommit(double timeline_time);

  unsigned SequenceNumber() const override { return sequence_number_; }
  int CompositorGroup() const { return compositor_group_; }

  static bool HasLowerPriority(const Animation* animation1,
                               const Animation* animation2) {
    return animation1->SequenceNumber() < animation2->SequenceNumber();
  }

  bool EffectSuppressed() const override { return effect_suppressed_; }
  void SetEffectSuppressed(bool);

  void InvalidateKeyframeEffect(const TreeScope&);

  void Trace(blink::Visitor*) override;

  bool CompositorPendingForTesting() const { return compositor_pending_; }

 protected:
  DispatchEventResult DispatchEventInternal(Event&) override;
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  void ClearOutdated();
  void ForceServiceOnNextFrame();

  double EffectEnd() const;
  bool Limited(double current_time) const;

  // Playback rate that will take effect once any pending tasks are resolved.
  // If there are no pending tasks, then the effective playback rate equals the
  // active playback rate.
  double EffectivePlaybackRate() const;
  void ResolvePendingPlaybackRate();

  // https://drafts.csswg.org/web-animations/#play-states
  // Per spec the viable states are: idle, running, paused and finished.
  // Our implementation has an additional state called 'pending' which serves a
  // similar purpose to micro-tasks in the spec. This additional state is for
  // internal flow control only and should not be reported via
  // animation.playState.
  // TODO(crbug.com/958433): Cleanup implementation to better align with the
  // spec.
  AnimationPlayState CalculatePlayState() const;
  // Spec compliant variant of play state calculation that is reported via
  // animation.playState.
  AnimationPlayState CalculateAnimationPlayState() const;

  base::Optional<double> CalculateStartTime(double current_time) const;
  double CalculateCurrentTime() const;

  void UnpauseInternal();
  void SetPlaybackRateInternal(double);
  void SetStartTimeInternal(base::Optional<double>);
  void UpdateCurrentTimingState(TimingUpdateReason);

  void BeginUpdatingState();
  void EndUpdatingState();

  CompositorAnimations::FailureReasons
  CheckCanStartAnimationOnCompositorInternal() const;
  void CreateCompositorAnimation();
  void DestroyCompositorAnimation();
  void AttachCompositorTimeline();
  void DetachCompositorTimeline();
  void AttachCompositedLayers();
  void DetachCompositedLayers();
  // CompositorAnimationDelegate implementation.
  void NotifyAnimationStarted(double monotonic_time, int group) override;
  void NotifyAnimationFinished(double monotonic_time, int group) override {}
  void NotifyAnimationAborted(double monotonic_time, int group) override {}

  using AnimationPromise = ScriptPromiseProperty<Member<Animation>,
                                                 Member<Animation>,
                                                 Member<DOMException>>;
  void ResolvePromiseMaybeAsync(AnimationPromise*);
  void RejectAndResetPromise(AnimationPromise*);
  void RejectAndResetPromiseMaybeAsync(AnimationPromise*);

  void QueueFinishedEvent();

  void ResetPendingTasks();
  double TimelineTime() const;
  DocumentTimeline& TickingTimeline();

  String id_;

  // Extended play state with additional pending state for managing timing of
  // micro-tasks.
  AnimationPlayState internal_play_state_;
  // Web exposed play state, which does not have pending state.
  AnimationPlayState animation_play_state_;
  double playback_rate_;
  // Playback rate that is currently in effect if differing from playback_rate_.
  // When playback_rate_ is modified, the new rate takes effect on the next
  // async tick. The currently active value is stored for use by the
  // Animation.playbackRate method.
  // TODO(crbug.com/960944): Switch to using pending_playback_rate_ once the
  // web-animations implementation is more closely aligned with the spec (i.e.
  // supports scheduling of pending tasks).
  base::Optional<double> active_playback_rate_;
  base::Optional<double> start_time_;
  base::Optional<double> hold_time_;

  unsigned sequence_number_;

  Member<AnimationPromise> finished_promise_;
  Member<AnimationPromise> ready_promise_;

  Member<AnimationEffect> content_;
  // Document refers to the timeline's document if there is a timeline.
  // Otherwise it refers to the document for the execution context.
  Member<Document> document_;
  Member<AnimationTimeline> timeline_;

  // Reflects all pausing, including via pauseForTesting().
  bool paused_;
  bool is_paused_for_testing_;
  bool is_composited_animation_disabled_for_testing_;

  // Pending micro-tasks. These flags are used for tracking purposes only for
  // the Animation.pending attribute, and do not otherwise affect internal flow
  // control.
  bool pending_pause_;
  bool pending_play_;

  // This indicates timing information relevant to the animation's effect
  // has changed by means other than the ordinary progression of time
  bool outdated_;

  bool finished_;
  // Holds a 'finished' event queued for asynchronous dispatch via the
  // ScriptedAnimationController. This object remains active until the
  // event is actually dispatched.
  Member<Event> pending_finished_event_;

  Member<Event> pending_cancelled_event_;

  enum CompositorAction { kNone, kPause, kStart, kPauseThenStart };

  class CompositorState {
    USING_FAST_MALLOC(CompositorState);

   public:
    explicit CompositorState(Animation& animation)
        : start_time(animation.start_time_),
          hold_time(animation.hold_time_),
          playback_rate(animation.playback_rate_),
          effect_changed(false),
          pending_action(kStart) {}
    base::Optional<double> start_time;
    base::Optional<double> hold_time;
    double playback_rate;
    bool effect_changed;
    CompositorAction pending_action;
    DISALLOW_COPY_AND_ASSIGN(CompositorState);
  };

  enum CompositorPendingChange {
    kSetCompositorPending,
    kSetCompositorPendingWithEffectChanged,
    kDoNotSetCompositorPending,
  };

  class PlayStateUpdateScope {
    STACK_ALLOCATED();

   public:
    PlayStateUpdateScope(Animation&,
                         TimingUpdateReason,
                         CompositorPendingChange = kSetCompositorPending);
    ~PlayStateUpdateScope();

   private:
    Member<Animation> animation_;
    AnimationPlayState initial_play_state_;
    CompositorPendingChange compositor_pending_change_;
  };

  // CompositorAnimation objects need to eagerly sever their connection to their
  // Animation delegate; use a separate 'holder' on-heap object to accomplish
  // that.
  class CompositorAnimationHolder
      : public GarbageCollectedFinalized<CompositorAnimationHolder> {
    USING_PRE_FINALIZER(CompositorAnimationHolder, Dispose);

   public:
    static CompositorAnimationHolder* Create(Animation*);

    explicit CompositorAnimationHolder(Animation*);

    void Detach();

    void Trace(blink::Visitor* visitor) { visitor->Trace(animation_); }

    CompositorAnimation* GetAnimation() const {
      return compositor_animation_.get();
    }

   private:
    void Dispose();

    std::unique_ptr<CompositorAnimation> compositor_animation_;
    Member<Animation> animation_;
  };

  // This mirrors the known compositor state. It is created when a compositor
  // animation is started. Updated once the start time is known and each time
  // modifications are pushed to the compositor.
  std::unique_ptr<CompositorState> compositor_state_;
  bool compositor_pending_;
  int compositor_group_;

  Member<CompositorAnimationHolder> compositor_animation_;

  bool current_time_pending_;
  bool state_is_being_updated_;

  bool effect_suppressed_;

  FRIEND_TEST_ALL_PREFIXES(AnimationAnimationTestCompositeAfterPaint,
                           NoCompositeWithoutCompositedElementId);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_H_
