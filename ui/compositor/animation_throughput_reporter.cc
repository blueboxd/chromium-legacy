// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/animation_throughput_reporter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cc/animation/animation.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/throughput_tracker.h"

namespace ui {

class AnimationThroughputReporter::AnimationTracker
    : public CallbackLayerAnimationObserver {
 public:
  AnimationTracker(LayerAnimator* animator, ReportCallback report_callback)
      : CallbackLayerAnimationObserver(
            base::BindRepeating(&AnimationTracker::OnAnimationEnded,
                                base::Unretained(this))),
        animator_(animator),
        report_callback_(std::move(report_callback)) {
    DCHECK(report_callback_);
  }
  AnimationTracker(const AnimationTracker& other) = delete;
  AnimationTracker& operator=(const AnimationTracker& other) = delete;
  ~AnimationTracker() override = default;

  // Whether there are attached animation sequences to track.
  bool IsTrackingAnimation() const { return !attached_sequences().empty(); }

  void set_should_delete(bool should_delete) { should_delete_ = should_delete; }

 private:
  // CallbackLayerAnimationObserver:
  void OnAnimatorAttachedToTimeline() override { MaybeStartTracking(); }
  void OnAnimatorDetachedFromTimeline() override {
    // Gives up tracking when detached from the timeline.
    first_animation_group_id_.reset();
    if (throughput_tracker_)
      throughput_tracker_.reset();

    // OnAnimationEnded would not happen after detached from the timeline.
    // So do the clean up here.
    if (should_delete_)
      delete this;
  }
  void OnLayerAnimationStarted(LayerAnimationSequence* sequence) override {
    CallbackLayerAnimationObserver::OnLayerAnimationStarted(sequence);

    if (!first_animation_group_id_.has_value()) {
      first_animation_group_id_ = sequence->animation_group_id();
      MaybeStartTracking();
    }

    // Make sure SetActive() is called so that OnAnimationEnded callback will be
    // invoked when all attached layer animation sequences finish.
    if (!active())
      SetActive();
  }
  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {
    // Check whether the aborted animation sequence is among the relevant ones
    // (started while the tracker is alive). This is done by checking the
    // animation_group_id() and assuming the id is monotonic increasing.
    if (first_animation_group_id_.has_value() &&
        first_animation_group_id_.value() <= sequence->animation_group_id()) {
      started_animations_aborted_ = true;
    }

    // Note the following call could delete |this|.
    CallbackLayerAnimationObserver::OnLayerAnimationAborted(sequence);
  }

  void MaybeStartTracking() {
    // No tracking if no layer animation sequence is started.
    if (!first_animation_group_id_.has_value())
      return;

    // No tracking if |animator_| is not attached to a timeline. Layer animation
    // sequence would not tick without a timeline.
    if (!AnimationThroughputReporter::IsAnimatorAttachedToTimeline(
            animator_.get())) {
      return;
    }

    ui::Compositor* compositor =
        AnimationThroughputReporter::GetCompositor(animator_.get());
    throughput_tracker_ = compositor->RequestNewThroughputTracker();
    throughput_tracker_->Start(report_callback_);
  }

  // Invoked when all animation sequences finish.
  bool OnAnimationEnded(const CallbackLayerAnimationObserver& self) {
    // |throughput_tracker| could reset when detached from animation timeline.
    if (throughput_tracker_) {
      if (started_animations_aborted_)
        throughput_tracker_->Cancel();
      else
        throughput_tracker_->Stop();
    }

    first_animation_group_id_.reset();
    started_animations_aborted_ = false;
    return should_delete_;
  }

  // Whether this class should delete itself on animation ended.
  bool should_delete_ = false;

  scoped_refptr<LayerAnimator> animator_;

  base::Optional<ThroughputTracker> throughput_tracker_;

  base::Optional<int> first_animation_group_id_;
  bool started_animations_aborted_ = false;

  AnimationThroughputReporter::ReportCallback report_callback_;
};

AnimationThroughputReporter::AnimationThroughputReporter(
    LayerAnimator* animator,
    ReportCallback report_callback)
    : animator_(animator),
      animation_tracker_(
          std::make_unique<AnimationTracker>(animator_,
                                             std::move(report_callback))) {
  animator_->AddObserver(animation_tracker_.get());
}

AnimationThroughputReporter::~AnimationThroughputReporter() {
  // Directly remove |animation_tracker_| from |LayerAnimator::observers_|
  // rather than calling LayerAnimator::RemoveObserver(), to avoid removing it
  // from the scheduled animation sequences.
  animator_->observers_.RemoveObserver(animation_tracker_.get());

  // |animation_tracker_| deletes itself when its tracked animations finish.
  if (animation_tracker_->IsTrackingAnimation())
    animation_tracker_.release()->set_should_delete(true);
}

// static
Compositor* AnimationThroughputReporter::GetCompositor(
    LayerAnimator* animator) {
  return animator->delegate()->GetLayer()->GetCompositor();
}

// static
bool AnimationThroughputReporter::IsAnimatorAttachedToTimeline(
    LayerAnimator* animator) {
  return animator->animation_->animation_timeline();
}

}  // namespace ui
