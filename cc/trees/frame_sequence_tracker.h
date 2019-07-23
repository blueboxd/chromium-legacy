// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_FRAME_SEQUENCE_TRACKER_H_
#define CC_TREES_FRAME_SEQUENCE_TRACKER_H_

#include <stdint.h>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/trace_event/traced_value.h"
#include "cc/cc_export.h"

namespace gfx {
struct PresentationFeedback;
}

namespace viz {
struct BeginFrameAck;
struct BeginFrameArgs;
}  // namespace viz

namespace cc {
class FrameSequenceTracker;

enum FrameSequenceTrackerType {
  kCompositorAnimation,
  kMainThreadAnimation,
  kPinchZoom,
  kRAF,
  kTouchScroll,
  kWheelScroll,
  kMaxType
};

// Used for notifying attached FrameSequenceTracker's of begin-frames and
// submitted frames.
class CC_EXPORT FrameSequenceTrackerCollection {
 public:
  FrameSequenceTrackerCollection();
  ~FrameSequenceTrackerCollection();

  FrameSequenceTrackerCollection(const FrameSequenceTrackerCollection&) =
      delete;
  FrameSequenceTrackerCollection& operator=(
      const FrameSequenceTrackerCollection&) = delete;

  // Creates a tracker for the specified sequence-type.
  std::unique_ptr<FrameSequenceTracker> CreateTracker(
      FrameSequenceTrackerType type);

  // Schedules |tracker| for destruction. This is preferred instead of outright
  // desrtruction of the tracker, since this ensures that the actual tracker
  // instance is destroyed *after* the presentation-feedbacks have been received
  // for all submitted frames.
  void ScheduleRemoval(std::unique_ptr<FrameSequenceTracker> tracker);

  // Removes all trackers. This also immediately destroys all trackers that had
  // been scheduled for destruction, even if there are pending
  // presentation-feedbacks. This is typically used if the client no longer
  // expects to receive presentation-feedbacks for the previously submitted
  // frames (e.g. when the gpu process dies).
  void ClearAll();

  // Notifies all trackers of various events.
  void NotifyBeginImplFrame(const viz::BeginFrameArgs& args);
  void NotifyBeginMainFrame(const viz::BeginFrameArgs& args);
  void NotifyImplFrameCausedNoDamage(const viz::BeginFrameAck& ack);
  void NotifyMainFrameCausedNoDamage(const viz::BeginFrameArgs& args);
  void NotifyPauseFrameProduction();
  void NotifySubmitFrame(uint32_t frame_token,
                         const viz::BeginFrameAck& ack,
                         const viz::BeginFrameArgs& origin_args);

  // Note that this notifies the trackers of the presentation-feedbacks, and
  // destroys any tracker that had been scheduled for destruction (using
  // |ScheduleRemoval()|) if it has no more pending frames.
  void NotifyFramePresented(uint32_t frame_token,
                            const gfx::PresentationFeedback& feedback);

 private:
  void AddFrameTracker(FrameSequenceTracker* tracker);
  void RemoveFrameTracker(FrameSequenceTracker* tracker);

  std::vector<FrameSequenceTracker*> frame_trackers_;
  std::vector<std::unique_ptr<FrameSequenceTracker>> removal_trackers_;
};

// Tracks a sequence of frames to determine the throughput. It tracks this by
// tracking the vsync sequence-numbers (from |BeginFrameArgs::sequence_number|),
// and the presentation-timestamps (from |gfx::PresentationFeedback|). It also
// tracks which frames were expected to include update from the main-thread, and
// which presented frames did include updates from the main-thread.
// This object should be created through
// FrameSequenceTrackerCollection::CreateTracker() API.
class CC_EXPORT FrameSequenceTracker {
 public:
  enum class TerminationStatus {
    kActive,
    kScheduledForTermination,
    kReadyForTermination,
  };

  ~FrameSequenceTracker();

  FrameSequenceTracker(const FrameSequenceTracker&) = delete;
  FrameSequenceTracker& operator=(const FrameSequenceTracker&) = delete;

  // Notifies the tracker when the compositor thread starts to process a
  // BeginFrameArgs.
  void ReportBeginImplFrame(const viz::BeginFrameArgs& args);

  // Notifies the tracker when a BeginFrameArgs is dispatched to the main
  // thread.
  void ReportBeginMainFrame(const viz::BeginFrameArgs& args);

  // Notifies the tracker when the compositor submits a CompositorFrame.
  // |origin_args| represents the BeginFrameArgs that triggered the update from
  // the main-thread.
  void ReportSubmitFrame(uint32_t frame_token,
                         const viz::BeginFrameAck& ack,
                         const viz::BeginFrameArgs& origin_args);

  // Notifies the tracker of the presentation-feedback of a previously submitted
  // CompositorFrame with |frame_token|.
  void ReportFramePresented(uint32_t frame_token,
                            const gfx::PresentationFeedback& feedback);

  // Notifies the tracker that a CompositorFrame is not going to be submitted
  // for a particular BeginFrameArgs because it did not cause any damage (visual
  // change). Note that if a begin-main-frame was dispatched, then a separate
  // call to |ReportMainFrameCausedNoDamage()| is made to notify that the
  // main-thread did not cause any damage/updates.
  void ReportImplFrameCausedNoDamage(const viz::BeginFrameAck& ack);

  // Notifies the tracker that a |BeginFrameArgs| either was not dispatched to
  // the main-thread (because it did not ask for it), or that a |BeginFrameArgs|
  // that was dispatched to the main-thread did not cause any updates/damage.
  void ReportMainFrameCausedNoDamage(const viz::BeginFrameArgs& args);

  // Notifies that frame production has currently paused. This is typically used
  // for interactive frame-sequences, e.g. during touch-scroll.
  void PauseFrameProduction();

  TerminationStatus termination_status() const { return termination_status_; }

 private:
  friend class FrameSequenceTrackerCollection;

  FrameSequenceTracker(
      FrameSequenceTrackerType type,
      base::OnceCallback<void(FrameSequenceTracker*)> destroy_callback);

  void ScheduleTerminate() {
    termination_status_ = TerminationStatus::kScheduledForTermination;
  }

  struct TrackedFrameData {
    // Represents the |BeginFrameArgs::source_id| and
    // |BeginFrameArgs::sequence_number| fields of the last processed
    // BeginFrameArgs.
    uint64_t previous_source = 0;
    uint64_t previous_sequence = 0;

    // The difference in |BeginFrameArgs::sequence_number| fields of the last
    // two processed BeginFrameArgs.
    uint8_t previous_sequence_delta = 0;
  };

  struct ThroughputData {
    static std::unique_ptr<base::trace_event::TracedValue> ToTracedValue(
        const ThroughputData& impl,
        const ThroughputData& main);
    static void ReportHistogram(FrameSequenceTrackerType sequence_type,
                                const char* thread_name,
                                int metric_index,
                                const ThroughputData& data);
    // Tracks the number of frames that were expected to be shown during this
    // frame-sequence.
    uint32_t frames_expected = 0;

    // Tracks the number of frames that were actually presented to the user
    // during this frame-sequence.
    uint32_t frames_produced = 0;
  };

  void UpdateTrackedFrameData(TrackedFrameData* frame_data,
                              uint64_t source_id,
                              uint64_t sequence_number);

  bool ShouldIgnoreBeginFrameSource(uint64_t source_id) const;

  const FrameSequenceTrackerType type_;
  base::OnceCallback<void(FrameSequenceTracker*)> destroy_callback_;

  TerminationStatus termination_status_ = TerminationStatus::kActive;

  TrackedFrameData begin_impl_frame_data_;
  TrackedFrameData begin_main_frame_data_;

  ThroughputData impl_throughput_;
  ThroughputData main_throughput_;

  // Tracks the list of frame-tokens for compositor-frames that included new
  // updates from the main-thread, whose presentation-feedback have not been
  // received yet. When the presentation-feedback for a frame is received, the
  // corresponding frame-token is removed from this collection.
  base::circular_deque<uint32_t> main_frames_;

  // Keeps track of the sequence-number of the first received begin-main-frame.
  // This is used to ignore submitted frames that include updates from earlier
  // begin-main-frames.
  uint64_t first_received_main_sequence_ = 0;

  // Keeps track of the first submitted compositor-frame. This is used to ignore
  // reports from frames that were submitted before this tracker had been
  // created.
  uint32_t first_submitted_frame_ = 0;

  // Keeps track of the latest submitted compositor-frame, so that it can
  // determine when it has received presentation-feedback for submitted frames.
  // This is used to decide when to terminate this FrameSequenceTracker object.
  uint32_t last_submitted_frame_ = 0;

  // Keeps track of the last sequence-number that produced a frame from the
  // main-thread.
  uint64_t last_submitted_main_sequence_ = 0;
};

}  // namespace cc

#endif  // CC_TREES_FRAME_SEQUENCE_TRACKER_H_
