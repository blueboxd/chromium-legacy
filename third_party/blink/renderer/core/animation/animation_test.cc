/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include <memory>

#include "base/bits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class AnimationAnimationTestNoCompositing : public RenderingTest {
 public:
  AnimationAnimationTestNoCompositing()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    RenderingTest::SetUp();
    SetUpWithoutStartingTimeline();
    StartTimeline();
  }

  void SetUpWithoutStartingTimeline() {
    GetDocument().GetAnimationClock().ResetTimeForTesting();
    timeline = GetDocument().Timeline();
    timeline->ResetForTesting();
    animation = timeline->Play(nullptr);
    animation->setStartTime(0, false);
    animation->setEffect(MakeAnimation());
  }

  void StartTimeline() { SimulateFrame(0); }

  KeyframeEffectModelBase* MakeSimpleEffectModel() {
    PropertyHandle PropertyHandleOpacity(GetCSSPropertyOpacity());
    TransitionKeyframe* start_keyframe =
        TransitionKeyframe::Create(PropertyHandleOpacity);
    start_keyframe->SetValue(std::make_unique<TypedInterpolationValue>(
        CSSNumberInterpolationType(PropertyHandleOpacity),
        std::make_unique<InterpolableNumber>(1.0)));
    start_keyframe->SetOffset(0.0);
    // Egregious hack: Sideload the compositor value.
    // This is usually set in a part of the rendering process SimulateFrame
    // doesn't call.
    start_keyframe->SetCompositorValue(CompositorKeyframeDouble::Create(1.0));
    TransitionKeyframe* end_keyframe =
        TransitionKeyframe::Create(PropertyHandleOpacity);
    end_keyframe->SetValue(std::make_unique<TypedInterpolationValue>(
        CSSNumberInterpolationType(PropertyHandleOpacity),
        std::make_unique<InterpolableNumber>(0.0)));
    end_keyframe->SetOffset(1.0);
    // Egregious hack: Sideload the compositor value.
    end_keyframe->SetCompositorValue(CompositorKeyframeDouble::Create(0.0));

    TransitionKeyframeVector keyframes;
    keyframes.push_back(start_keyframe);
    keyframes.push_back(end_keyframe);

    return MakeGarbageCollected<TransitionKeyframeEffectModel>(keyframes);
  }

  void ResetWithCompositedAnimation() {
    // Get rid of the default animation.
    animation->cancel();

    RunDocumentLifecycle();

    SetBodyInnerHTML("<div id='target'></div>");

    // Create a compositable animation; in this case opacity from 1 to 0.
    Timing timing;
    timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

    Persistent<StringKeyframe> start_keyframe =
        MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                        SecureContextMode::kInsecureContext,
                                        nullptr);
    Persistent<StringKeyframe> end_keyframe =
        MakeGarbageCollected<StringKeyframe>();
    end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);

    StringKeyframeVector keyframes;
    keyframes.push_back(start_keyframe);
    keyframes.push_back(end_keyframe);

    Element* element = GetElementById("target");
    auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
    animation = timeline->Play(
        MakeGarbageCollected<KeyframeEffect>(element, model, timing));

    // After creating the animation we need to clean the lifecycle so that the
    // animation can be pushed to the compositor.
    UpdateAllLifecyclePhasesForTest();

    GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks());
    GetDocument().GetPendingAnimations().Update(nullptr, true);
  }

  KeyframeEffectModelBase* MakeEmptyEffectModel() {
    return MakeGarbageCollected<StringKeyframeEffectModel>(
        StringKeyframeVector());
  }

  KeyframeEffect* MakeAnimation(double duration = 30) {
    Timing timing;
    timing.iteration_duration = AnimationTimeDelta::FromSecondsD(duration);
    return MakeGarbageCollected<KeyframeEffect>(nullptr, MakeEmptyEffectModel(),
                                                timing);
  }

  bool SimulateFrame(double time) {
    const PaintArtifactCompositor* paint_artifact_compositor = nullptr;
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
        RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
      paint_artifact_compositor = GetDocument()
                                      .GetFrame()
                                      ->View()
                                      ->GetPaintArtifactCompositorForTesting();
    }
    GetDocument().GetAnimationClock().UpdateTime(
        base::TimeTicks() + base::TimeDelta::FromSecondsD(time));
    GetDocument().GetPendingAnimations().Update(paint_artifact_compositor,
                                                false);
    // The timeline does not know about our animation, so we have to explicitly
    // call update().
    return animation->Update(kTimingUpdateForAnimationFrame);
  }

  Persistent<DocumentTimeline> timeline;
  Persistent<Animation> animation;
};

class AnimationAnimationTestCompositing
    : public AnimationAnimationTestNoCompositing {
  void SetUp() override {
    EnableCompositing();
    AnimationAnimationTestNoCompositing::SetUp();
  }
};

class AnimationAnimationTestCompositeAfterPaint
    : public AnimationAnimationTestNoCompositing {
  void SetUp() override {
    EnableCompositing();
    AnimationAnimationTestNoCompositing::SetUp();
  }

  ScopedCompositeAfterPaintForTest enable_cap{true};
};

TEST_F(AnimationAnimationTestNoCompositing, InitialState) {
  SetUpWithoutStartingTimeline();
  animation = timeline->Play(nullptr);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_FALSE(animation->Paused());
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_FALSE(animation->StartTimeInternal());

  StartTimeline();
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(0, timeline->CurrentTimeInternal());
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_FALSE(animation->Paused());
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_EQ(0, animation->StartTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, CurrentTimeDoesNotSetOutdated) {
  EXPECT_FALSE(animation->Outdated());
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_FALSE(animation->Outdated());
  // FIXME: We should split simulateFrame into a version that doesn't update
  // the animation and one that does, as most of the tests don't require
  // update() to be called.
  GetDocument().GetAnimationClock().UpdateTime(
      base::TimeTicks() + base::TimeDelta::FromSecondsD(10));
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  EXPECT_FALSE(animation->Outdated());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTime) {
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  animation->SetCurrentTimeInternal(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(20, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimeNegative) {
  animation->SetCurrentTimeInternal(-10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(-10, animation->CurrentTimeInternal());
  SimulateFrame(20);
  EXPECT_EQ(10, animation->CurrentTimeInternal());

  animation->setPlaybackRate(-2);
  animation->SetCurrentTimeInternal(-10);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(-10, animation->CurrentTimeInternal());
  SimulateFrame(40);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(-10, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing,
       SetCurrentTimeNegativeWithoutSimultaneousPlaybackRateChange) {
  SimulateFrame(20);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  animation->setPlaybackRate(-1);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  SimulateFrame(30);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  animation->setCurrentTime(-10 * 1000, false);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimePastContentEnd) {
  animation->setCurrentTime(50 * 1000, false);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(50, animation->CurrentTimeInternal());
  SimulateFrame(20);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(50, animation->CurrentTimeInternal());

  animation->setPlaybackRate(-2);
  animation->setCurrentTime(50 * 1000, false);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(50, animation->CurrentTimeInternal());
  SimulateFrame(20);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  SimulateFrame(40);
  EXPECT_EQ(10, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimeMax) {
  animation->SetCurrentTimeInternal(std::numeric_limits<double>::max());
  EXPECT_EQ(std::numeric_limits<double>::max(),
            animation->CurrentTimeInternal());
  SimulateFrame(100);
  EXPECT_EQ(std::numeric_limits<double>::max(),
            animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimeSetsStartTime) {
  EXPECT_EQ(0, animation->startTime());
  animation->setCurrentTime(1000, false);
  EXPECT_EQ(-1000, animation->startTime());
  SimulateFrame(1);
  EXPECT_EQ(-1000, animation->startTime());
  EXPECT_EQ(2000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetStartTime) {
  SimulateFrame(20);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(0, animation->StartTimeInternal());
  EXPECT_EQ(20 * 1000, animation->currentTime());
  animation->setStartTime(10 * 1000, false);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(10, animation->StartTimeInternal());
  EXPECT_EQ(10 * 1000, animation->currentTime());
  SimulateFrame(30);
  EXPECT_EQ(10, animation->StartTimeInternal());
  EXPECT_EQ(20 * 1000, animation->currentTime());
  animation->setStartTime(-20 * 1000, false);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetStartTimeLimitsAnimation) {
  animation->setStartTime(-50 * 1000, false);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  animation->setPlaybackRate(-1);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  animation->setStartTime(-100 * 1000, false);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_TRUE(animation->Limited());
}

TEST_F(AnimationAnimationTestNoCompositing, SetStartTimeOnLimitedAnimation) {
  SimulateFrame(30);
  animation->setStartTime(-10 * 1000, false);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  animation->SetCurrentTimeInternal(50);
  animation->setStartTime(-40 * 1000, false);
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_TRUE(animation->Limited());
}

TEST_F(AnimationAnimationTestNoCompositing, StartTimePauseFinish) {
  NonThrowableExceptionState exception_state;
  animation->pause();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_FALSE(animation->startTime());
  animation->finish(exception_state);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
  EXPECT_EQ(-30000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishWhenPaused) {
  NonThrowableExceptionState exception_state;
  animation->pause();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kPaused, animation->PlayStateInternal());
  animation->finish(exception_state);
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, StartTimeFinishPause) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  EXPECT_EQ(-30 * 1000, animation->startTime());
  animation->pause();
  EXPECT_FALSE(animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, StartTimeWithZeroPlaybackRate) {
  animation->setPlaybackRate(0);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_FALSE(animation->startTime());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, PausePlay) {
  SimulateFrame(10);
  animation->pause();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_TRUE(animation->Paused());
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  SimulateFrame(20);
  EXPECT_EQ(Animation::kPaused, animation->PlayStateInternal());
  animation->play();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  SimulateFrame(20);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_FALSE(animation->Paused());
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  SimulateFrame(30);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, PlayRewindsToStart) {
  animation->SetCurrentTimeInternal(30);
  animation->play();
  EXPECT_EQ(0, animation->CurrentTimeInternal());

  animation->SetCurrentTimeInternal(40);
  animation->play();
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());

  animation->SetCurrentTimeInternal(-10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  animation->play();
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, PlayRewindsToEnd) {
  animation->setPlaybackRate(-1);
  animation->play();
  EXPECT_EQ(30, animation->CurrentTimeInternal());

  animation->SetCurrentTimeInternal(40);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  animation->play();
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());

  animation->SetCurrentTimeInternal(-10);
  animation->play();
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  SimulateFrame(20);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing,
       PlayWithPlaybackRateZeroDoesNotSeek) {
  animation->setPlaybackRate(0);
  animation->play();
  EXPECT_EQ(0, animation->CurrentTimeInternal());

  animation->SetCurrentTimeInternal(40);
  animation->play();
  EXPECT_EQ(40, animation->CurrentTimeInternal());

  animation->SetCurrentTimeInternal(-10);
  animation->play();
  EXPECT_EQ(-10, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing,
       PlayAfterPauseWithPlaybackRateZeroUpdatesPlayState) {
  animation->pause();
  animation->setPlaybackRate(0);
  SimulateFrame(1);
  EXPECT_EQ(Animation::kPaused, animation->PlayStateInternal());
  animation->play();
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, Reverse) {
  animation->SetCurrentTimeInternal(10);
  animation->pause();
  animation->reverse();
  EXPECT_FALSE(animation->Paused());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  SimulateFrame(0);
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(-1, animation->playbackRate());
}

TEST_F(AnimationAnimationTestNoCompositing,
       ReverseDoesNothingWithPlaybackRateZero) {
  animation->SetCurrentTimeInternal(10);
  animation->setPlaybackRate(0);
  animation->pause();
  animation->reverse();
  EXPECT_TRUE(animation->Paused());
  EXPECT_EQ(0, animation->playbackRate());
  EXPECT_EQ(10, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseSeeksToStart) {
  animation->SetCurrentTimeInternal(-10);
  animation->setPlaybackRate(-1);
  animation->reverse();
  EXPECT_EQ(0, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseSeeksToEnd) {
  animation->setCurrentTime(40 * 1000, false);
  animation->reverse();
  EXPECT_EQ(30, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseBeyondLimit) {
  animation->SetCurrentTimeInternal(40);
  animation->setPlaybackRate(-1);
  animation->reverse();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(0, animation->CurrentTimeInternal());

  animation->SetCurrentTimeInternal(-10);
  animation->reverse();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(30, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, Finish) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());

  animation->setPlaybackRate(-1);
  animation->finish(exception_state);
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishAfterEffectEnd) {
  NonThrowableExceptionState exception_state;
  animation->setCurrentTime(40 * 1000, false);
  animation->finish(exception_state);
  EXPECT_EQ(40, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishBeforeStart) {
  NonThrowableExceptionState exception_state;
  animation->SetCurrentTimeInternal(-10);
  animation->setPlaybackRate(-1);
  animation->finish(exception_state);
  EXPECT_EQ(0, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing,
       FinishDoesNothingWithPlaybackRateZero) {
  DummyExceptionStateForTesting exception_state;
  animation->SetCurrentTimeInternal(10);
  animation->setPlaybackRate(0);
  animation->finish(exception_state);
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishRaisesException) {
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(1);
  timing.iteration_count = std::numeric_limits<double>::infinity();
  animation->setEffect(MakeGarbageCollected<KeyframeEffect>(
      nullptr, MakeEmptyEffectModel(), timing));
  animation->SetCurrentTimeInternal(10);

  DummyExceptionStateForTesting exception_state;
  animation->finish(exception_state);
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            exception_state.CodeAs<DOMExceptionCode>());
}

TEST_F(AnimationAnimationTestNoCompositing, LimitingAtEffectEnd) {
  SimulateFrame(30);
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(40);
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  EXPECT_FALSE(animation->Paused());
}

TEST_F(AnimationAnimationTestNoCompositing, LimitingAtStart) {
  SimulateFrame(30);
  animation->setPlaybackRate(-2);
  SimulateFrame(30);
  SimulateFrame(45);
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(60);
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  EXPECT_FALSE(animation->Paused());
}

TEST_F(AnimationAnimationTestNoCompositing, LimitingWithNoEffect) {
  animation->setEffect(nullptr);
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(30);
  EXPECT_EQ(0, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRate) {
  animation->setPlaybackRate(2);
  SimulateFrame(0);
  EXPECT_EQ(2, animation->playbackRate());
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  SimulateFrame(10);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateWhilePaused) {
  SimulateFrame(10);
  animation->pause();
  animation->setPlaybackRate(2);
  SimulateFrame(20);
  animation->play();
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  SimulateFrame(20);
  SimulateFrame(25);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateWhileLimited) {
  SimulateFrame(40);
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  animation->setPlaybackRate(2);
  SimulateFrame(50);
  EXPECT_EQ(30, animation->CurrentTimeInternal());
  animation->setPlaybackRate(-2);
  SimulateFrame(50);
  SimulateFrame(60);
  EXPECT_FALSE(animation->Limited());
  EXPECT_EQ(10, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateZero) {
  SimulateFrame(0);
  SimulateFrame(10);
  animation->setPlaybackRate(0);
  SimulateFrame(10);
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  SimulateFrame(20);
  EXPECT_EQ(10, animation->CurrentTimeInternal());
  animation->SetCurrentTimeInternal(20);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateMax) {
  animation->setPlaybackRate(std::numeric_limits<double>::max());
  SimulateFrame(0);
  EXPECT_EQ(std::numeric_limits<double>::max(), animation->playbackRate());
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  SimulateFrame(1);
  EXPECT_EQ(30, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRate) {
  animation->updatePlaybackRate(2);
  EXPECT_EQ(1, animation->playbackRate());

  SimulateFrame(0);
  EXPECT_EQ(2, animation->playbackRate());
  EXPECT_EQ(0, animation->CurrentTimeInternal());

  SimulateFrame(10);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhilePaused) {
  animation->pause();

  // Pending playback rate on pending-paused animation is picked up after async
  // tick.
  EXPECT_TRUE(animation->Paused());
  EXPECT_TRUE(animation->pending());
  animation->updatePlaybackRate(2);
  EXPECT_EQ(1, animation->playbackRate());

  SimulateFrame(1);
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(2, animation->playbackRate());

  // Pending playback rate on a paused animation is resolved immediately.
  animation->updatePlaybackRate(3);
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(3, animation->playbackRate());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhileLimited) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  EXPECT_EQ(30, animation->CurrentTimeInternal());

  // Updating playback rate does not affect current time.
  animation->updatePlaybackRate(2);
  EXPECT_EQ(30, animation->CurrentTimeInternal());

  // Updating payback rate is resolved immediately for an animation in the
  // finished state.
  EXPECT_EQ(2, animation->playbackRate());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhileRunning) {
  animation->play();
  SimulateFrame(1);
  animation->updatePlaybackRate(2);

  // Updating playback rate triggers pending state for the play state.
  // Pending playback rate is not resolved until next async tick.
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(1, animation->playbackRate());

  SimulateFrame(1);
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(2, animation->playbackRate());
}

TEST_F(AnimationAnimationTestNoCompositing, SetEffect) {
  animation = timeline->Play(nullptr);
  animation->setStartTime(0, false);
  AnimationEffect* effect1 = MakeAnimation();
  AnimationEffect* effect2 = MakeAnimation();
  animation->setEffect(effect1);
  EXPECT_EQ(effect1, animation->effect());
  EXPECT_EQ(0, animation->CurrentTimeInternal());
  animation->SetCurrentTimeInternal(15);
  animation->setEffect(effect2);
  EXPECT_EQ(15, animation->CurrentTimeInternal());
  EXPECT_EQ(nullptr, effect1->GetAnimationForTesting());
  EXPECT_EQ(animation, effect2->GetAnimationForTesting());
  EXPECT_EQ(effect2, animation->effect());
}

TEST_F(AnimationAnimationTestNoCompositing, SetEffectLimitsAnimation) {
  animation->SetCurrentTimeInternal(20);
  animation->setEffect(MakeAnimation(10));
  EXPECT_EQ(20, animation->CurrentTimeInternal());
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(10);
  EXPECT_EQ(20, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, SetEffectUnlimitsAnimation) {
  animation->SetCurrentTimeInternal(40);
  animation->setEffect(MakeAnimation(60));
  EXPECT_FALSE(animation->Limited());
  EXPECT_EQ(40, animation->CurrentTimeInternal());
  SimulateFrame(10);
  EXPECT_EQ(50, animation->CurrentTimeInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, EmptyAnimationsDontUpdateEffects) {
  animation = timeline->Play(nullptr);
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());

  SimulateFrame(1234);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing, AnimationsDisassociateFromEffect) {
  AnimationEffect* animation_node = animation->effect();
  Animation* animation2 = timeline->Play(animation_node);
  EXPECT_EQ(nullptr, animation->effect());
  animation->setEffect(animation_node);
  EXPECT_EQ(nullptr, animation2->effect());
}

TEST_F(AnimationAnimationTestNoCompositing, AnimationsReturnTimeToNextEffect) {
  Timing timing;
  timing.start_delay = 1;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(1);
  timing.end_delay = 1;
  auto* keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      nullptr, MakeEmptyEffectModel(), timing);
  animation = timeline->Play(keyframe_effect);
  animation->setStartTime(0, false);

  // Next effect change at end of start delay.
  SimulateFrame(0);
  EXPECT_EQ(1, animation->TimeToEffectChange());

  // Next effect change at end of start delay.
  SimulateFrame(0.5);
  EXPECT_EQ(0.5, animation->TimeToEffectChange());

  // Start of active phase.
  SimulateFrame(1);
  EXPECT_EQ(0, animation->TimeToEffectChange());

  // Still in active phase.
  SimulateFrame(1.5);
  EXPECT_EQ(0, animation->TimeToEffectChange());

  // Start of the after phase. Next effect change at end of after phase.
  SimulateFrame(2);
  EXPECT_EQ(1, animation->TimeToEffectChange());

  // Still in effect if fillmode = forward|both.
  SimulateFrame(3);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());

  // Reset to start of animation. Next effect at the end of the start delay.
  animation->SetCurrentTimeInternal(0);
  SimulateFrame(3);
  EXPECT_EQ(1, animation->TimeToEffectChange());

  // Start delay is scaled by playback rate.
  animation->setPlaybackRate(2);
  SimulateFrame(3);
  EXPECT_EQ(0.5, animation->TimeToEffectChange());

  // Effectively a paused animation.
  animation->setPlaybackRate(0);
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());

  // Reversed animation from end time. Next effect after end delay.
  animation->SetCurrentTimeInternal(3);
  animation->setPlaybackRate(-1);
  animation->Update(kTimingUpdateOnDemand);
  SimulateFrame(3);
  EXPECT_EQ(1, animation->TimeToEffectChange());

  // End delay is scaled by playback rate.
  animation->setPlaybackRate(-2);
  animation->Update(kTimingUpdateOnDemand);
  SimulateFrame(3);
  EXPECT_EQ(0.5, animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing, TimeToNextEffectWhenPaused) {
  EXPECT_EQ(0, animation->TimeToEffectChange());
  animation->pause();
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing,
       TimeToNextEffectWhenCancelledBeforeStart) {
  EXPECT_EQ(0, animation->TimeToEffectChange());
  animation->SetCurrentTimeInternal(-8);
  animation->setPlaybackRate(2);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  animation->cancel();
  EXPECT_EQ(Animation::kIdle, animation->PlayStateInternal());
  animation->Update(kTimingUpdateOnDemand);
  // This frame will fire the finish event event though no start time has been
  // received from the compositor yet, as cancel() nukes start times.
  SimulateFrame(0);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing,
       TimeToNextEffectWhenCancelledBeforeStartReverse) {
  EXPECT_EQ(0, animation->TimeToEffectChange());
  animation->SetCurrentTimeInternal(9);
  animation->setPlaybackRate(-3);
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  animation->cancel();
  EXPECT_EQ(Animation::kIdle, animation->PlayStateInternal());
  animation->Update(kTimingUpdateOnDemand);
  // This frame will fire the finish event event though no start time has been
  // received from the compositor yet, as cancel() nukes start times.
  SimulateFrame(0);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing,
       TimeToNextEffectSimpleCancelledBeforeStart) {
  EXPECT_EQ(0, animation->TimeToEffectChange());
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  animation->cancel();
  animation->Update(kTimingUpdateOnDemand);
  // This frame will fire the finish event event though no start time has been
  // received from the compositor yet, as cancel() nukes start times.
  SimulateFrame(0);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing, AttachedAnimations) {
  Persistent<Element> element = GetDocument().CreateElementForBinding("foo");

  Timing timing;
  auto* keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      element.Get(), MakeEmptyEffectModel(), timing);
  Animation* animation = timeline->Play(keyframe_effect);
  SimulateFrame(0);
  timeline->ServiceAnimations(kTimingUpdateForAnimationFrame);
  EXPECT_EQ(
      1U, element->GetElementAnimations()->Animations().find(animation)->value);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(element->GetElementAnimations()->Animations().IsEmpty());
}

TEST_F(AnimationAnimationTestNoCompositing, HasLowerPriority) {
  Animation* animation1 = timeline->Play(nullptr);
  Animation* animation2 = timeline->Play(nullptr);
  EXPECT_TRUE(Animation::HasLowerPriority(animation1, animation2));
}

TEST_F(AnimationAnimationTestNoCompositing, PlayAfterCancel) {
  animation->cancel();
  EXPECT_EQ(Animation::kIdle, animation->PlayStateInternal());
  EXPECT_TRUE(std::isnan(animation->currentTime()));
  EXPECT_FALSE(animation->startTime());
  animation->play();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->startTime());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_EQ(10 * 1000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, PlayBackwardsAfterCancel) {
  animation->setPlaybackRate(-1);
  animation->setCurrentTime(15 * 1000, false);
  SimulateFrame(0);
  animation->cancel();
  EXPECT_EQ(Animation::kIdle, animation->PlayStateInternal());
  EXPECT_TRUE(std::isnan(animation->currentTime()));
  EXPECT_FALSE(animation->startTime());
  animation->play();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(30 * 1000, animation->currentTime());
  EXPECT_FALSE(animation->startTime());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(30 * 1000, animation->currentTime());
  EXPECT_EQ(40 * 1000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseAfterCancel) {
  animation->cancel();
  EXPECT_EQ(Animation::kIdle, animation->PlayStateInternal());
  EXPECT_TRUE(std::isnan(animation->currentTime()));
  EXPECT_FALSE(animation->startTime());
  animation->reverse();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(30 * 1000, animation->currentTime());
  EXPECT_FALSE(animation->startTime());
  SimulateFrame(10);
  EXPECT_EQ(Animation::kRunning, animation->PlayStateInternal());
  EXPECT_EQ(30 * 1000, animation->currentTime());
  EXPECT_EQ(40 * 1000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishAfterCancel) {
  NonThrowableExceptionState exception_state;
  animation->cancel();
  EXPECT_EQ(Animation::kIdle, animation->PlayStateInternal());
  EXPECT_TRUE(std::isnan(animation->currentTime()));
  EXPECT_FALSE(animation->startTime());
  animation->finish(exception_state);
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_EQ(-30000, animation->startTime());
  EXPECT_EQ(Animation::kFinished, animation->PlayStateInternal());
}

TEST_F(AnimationAnimationTestNoCompositing, PauseAfterCancel) {
  animation->cancel();
  EXPECT_EQ(Animation::kIdle, animation->PlayStateInternal());
  EXPECT_TRUE(std::isnan(animation->currentTime()));
  EXPECT_FALSE(animation->startTime());
  animation->pause();
  EXPECT_EQ(Animation::kPending, animation->PlayStateInternal());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->startTime());
}

TEST_F(AnimationAnimationTestCompositeAfterPaint,
       NoCompositeWithoutCompositedElementId) {
  SetBodyInnerHTML(
      "<div id='foo' style='position: relative; will-change: "
      "opacity;'>composited</div>"
      "<div id='bar' style='position: relative'>not composited</div>");

  LayoutObject* object_composited = GetLayoutObjectByElementId("foo");
  LayoutObject* object_not_composited = GetLayoutObjectByElementId("bar");

  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  auto* keyframe_effect_composited = MakeGarbageCollected<KeyframeEffect>(
      To<Element>(object_composited->GetNode()), MakeSimpleEffectModel(),
      timing);
  Animation* animation_composited = timeline->Play(keyframe_effect_composited);
  auto* keyframe_effect_not_composited = MakeGarbageCollected<KeyframeEffect>(
      To<Element>(object_not_composited->GetNode()), MakeSimpleEffectModel(),
      timing);
  Animation* animation_not_composited =
      timeline->Play(keyframe_effect_not_composited);

  SimulateFrame(0);
  EXPECT_EQ(animation_composited->CheckCanStartAnimationOnCompositorInternal(),
            CompositorAnimations::kNoFailure);
  const PaintArtifactCompositor* paint_artifact_compositor =
      GetDocument().View()->GetPaintArtifactCompositor();
  ASSERT_TRUE(paint_artifact_compositor);
  EXPECT_EQ(animation_composited->CheckCanStartAnimationOnCompositor(
                paint_artifact_compositor),
            CompositorAnimations::kNoFailure);
  EXPECT_NE(animation_not_composited->CheckCanStartAnimationOnCompositor(
                paint_artifact_compositor),
            CompositorAnimations::kNoFailure);
}

// Regression test for http://crbug.com/819591 . If a compositable animation is
// played and then paused before any start time is set (either blink or
// compositor side), the pausing must still set compositor pending or the pause
// won't be synced.
TEST_F(AnimationAnimationTestCompositing,
       SetCompositorPendingWithUnresolvedStartTimes) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPendingForTesting());

  // However, if we pause the animation then the compositor should still be
  // marked pending. This is required because otherwise the compositor will go
  // ahead and start playing the animation once it receives a start time (e.g.
  // on the next compositor frame).
  animation->pause();

  EXPECT_TRUE(animation->CompositorPendingForTesting());
}

TEST_F(AnimationAnimationTestCompositing, PreCommitWithUnresolvedStartTimes) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPendingForTesting());

  // At this point, a call to PreCommit should bail out and tell us to wait for
  // next commit because there are no resolved start times.
  EXPECT_FALSE(animation->PreCommit(0, nullptr, true));
}

namespace {
int GenerateHistogramValue(CompositorAnimations::FailureReason reason) {
  // The enum values in CompositorAnimations::FailureReasons are stored as 2^i
  // as they are a bitmask, but are recorded into the histogram as (i+1) to give
  // sequential histogram values. The exception is kNoFailure, which is stored
  // as 0 and recorded as 0.
  if (reason == CompositorAnimations::kNoFailure)
    return CompositorAnimations::kNoFailure;
  return base::bits::CountTrailingZeroBits(static_cast<uint32_t>(reason)) + 1;
}
}  // namespace

TEST_F(AnimationAnimationTestCompositing, PreCommitRecordsHistograms) {
  const std::string histogram_name =
      "Blink.Animation.CompositedAnimationFailureReason";

  // Initially the animation in this test has no target, so it is invalid.
  {
    HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kInvalidAnimationOrEffect),
        1);
  }

  // Restart the animation with a target and compositing state.
  {
    HistogramTester histogram;
    ResetWithCompositedAnimation();
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kNoFailure), 1);
  }

  // Now make the playback rate 0. This trips both the invalid animation and
  // unsupported timing parameter reasons.
  animation->setPlaybackRate(0);
  animation->NotifyCompositorStartTime(100);
  {
    HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kInvalidAnimationOrEffect),
        1);
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(
            CompositorAnimations::kEffectHasUnsupportedTimingParameters),
        1);
  }
  animation->setPlaybackRate(1);

  // Finally, change the keyframes to something unsupported by the compositor.
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      CSSPropertyID::kLeft, "0", SecureContextMode::kInsecureContext, nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kLeft, "100px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  ToKeyframeEffect(animation->effect())
      ->SetKeyframes({start_keyframe, end_keyframe});
  UpdateAllLifecyclePhasesForTest();
  {
    HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kUnsupportedCSSProperty),
        1);
  }
}

TEST_F(AnimationAnimationTestCompositing, SetKeyframesCausesCompositorPending) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPendingForTesting());

  // Now change the keyframes; this should mark the animation as compositor
  // pending as we need to sync the compositor side.
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  ToKeyframeEffect(animation->effect())->SetKeyframes(keyframes);

  EXPECT_TRUE(animation->CompositorPendingForTesting());
}

// Verifies correctness of scroll linked animation current and start times in
// various animation states.
TEST_F(AnimationAnimationTestNoCompositing, ScrollLinkedAnimationCreation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20), kProgrammaticScroll);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  NonThrowableExceptionState exception_state;
  Animation* scroll_animation =
      Animation::Create(MakeAnimation(), scroll_timeline, exception_state);

  // Verify start and current times in Idle state.
  bool is_null;
  scroll_animation->startTime(is_null);
  EXPECT_TRUE(is_null);
  scroll_animation->currentTime(is_null);
  EXPECT_TRUE(is_null);

  scroll_animation->play();

  // Verify start and current times in Pending state.
  scroll_animation->startTime(is_null);
  EXPECT_TRUE(is_null);
  EXPECT_EQ(0, scroll_animation->currentTime(is_null));
  EXPECT_FALSE(is_null);

  UpdateAllLifecyclePhasesForTest();
  // Verify start and current times in Playing state.
  EXPECT_EQ(0, scroll_animation->startTime(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_EQ(20, scroll_animation->currentTime(is_null));
  EXPECT_FALSE(is_null);

  // Verify current time after scroll.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 40), kProgrammaticScroll);
  EXPECT_EQ(40, scroll_animation->currentTime(is_null));
  EXPECT_FALSE(is_null);
}

}  // namespace blink
