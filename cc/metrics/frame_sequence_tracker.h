// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_FRAME_SEQUENCE_TRACKER_H_
#define CC_METRICS_FRAME_SEQUENCE_TRACKER_H_

#include <stdint.h>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/trace_event/traced_value.h"
#include "cc/cc_export.h"

namespace gfx {
struct PresentationFeedback;
}

namespace viz {
struct BeginFrameAck;
struct BeginFrameArgs;
struct BeginFrameId;
}  // namespace viz

namespace cc {
class FrameSequenceTracker;
class CompositorFrameReportingController;
class ThroughputUkmReporter;
class UkmManager;

enum class FrameSequenceTrackerType {
  // Used as an enum for metrics. DO NOT reorder or delete values. Rather,
  // add them at the end and increment kMaxType.
  kCompositorAnimation = 0,
  kMainThreadAnimation = 1,
  kPinchZoom = 2,
  kRAF = 3,
  kTouchScroll = 4,
  kUniversal = 5,
  kVideo = 6,
  kWheelScroll = 7,
  kScrollbarScroll = 8,
  kCustom = 9,  // Note that the metrics for kCustom are not reported on UMA,
                // and instead are dispatched back to the LayerTreeHostClient.
  kMaxType
};

typedef uint16_t ActiveFrameSequenceTrackers;

class CC_EXPORT FrameSequenceMetrics {
 public:
  FrameSequenceMetrics(FrameSequenceTrackerType type,
                       ThroughputUkmReporter* ukm_reporter);
  ~FrameSequenceMetrics();

  FrameSequenceMetrics(const FrameSequenceMetrics&) = delete;
  FrameSequenceMetrics& operator=(const FrameSequenceMetrics&) = delete;

  enum class ThreadType { kMain, kCompositor, kSlower, kUnknown };

  struct ThroughputData {
    static std::unique_ptr<base::trace_event::TracedValue> ToTracedValue(
        const ThroughputData& impl,
        const ThroughputData& main);

    // Returns the throughput in percent, a return value of base::nullopt
    // indicates that no throughput metric is reported.
    static base::Optional<int> ReportHistogram(FrameSequenceMetrics* metrics,
                                               ThreadType thread_type,
                                               int metric_index,
                                               const ThroughputData& data);

    void Merge(const ThroughputData& data) {
      frames_expected += data.frames_expected;
      frames_produced += data.frames_produced;
#if DCHECK_IS_ON()
      frames_processed += data.frames_processed;
      frames_received += data.frames_received;
#endif
    }

    // Tracks the number of frames that were expected to be shown during this
    // frame-sequence.
    uint32_t frames_expected = 0;

    // Tracks the number of frames that were actually presented to the user
    // during this frame-sequence.
    uint32_t frames_produced = 0;

#if DCHECK_IS_ON()
    // Tracks the number of frames that is either submitted or reported as no
    // damage.
    uint32_t frames_processed = 0;

    // Tracks the number of begin-frames that are received.
    uint32_t frames_received = 0;
#endif
  };

  void SetScrollingThread(ThreadType thread);

  // Returns the 'effective thread' for the metrics (i.e. the thread most
  // relevant for this metric).
  ThreadType GetEffectiveThread() const;

  void Merge(std::unique_ptr<FrameSequenceMetrics> metrics);
  bool HasEnoughDataForReporting() const;
  bool HasDataLeftForReporting() const;
  // Report related metrics: throughput, checkboarding...
  void ReportMetrics();
  void ComputeAggregatedThroughputForTesting() {
    ComputeAggregatedThroughput();
  }

  ThroughputData& impl_throughput() { return impl_throughput_; }
  ThroughputData& main_throughput() { return main_throughput_; }
  ThroughputData& aggregated_throughput() { return aggregated_throughput_; }
  void add_checkerboarded_frames(int64_t frames) {
    frames_checkerboarded_ += frames;
  }
  uint32_t frames_checkerboarded() const { return frames_checkerboarded_; }

  FrameSequenceTrackerType type() const { return type_; }
  ThroughputUkmReporter* ukm_reporter() const {
    return throughput_ukm_reporter_;
  }

 private:
  void ComputeAggregatedThroughput();
  const FrameSequenceTrackerType type_;

  // Pointer to the reporter owned by the FrameSequenceTrackerCollection.
  ThroughputUkmReporter* const throughput_ukm_reporter_;

  ThroughputData impl_throughput_;
  ThroughputData main_throughput_;
  // The aggregated throughput for the main/compositor thread.
  ThroughputData aggregated_throughput_;

  ThreadType scrolling_thread_ = ThreadType::kUnknown;

  // Tracks the number of produced frames that had some amount of
  // checkerboarding, and how many frames showed such checkerboarded frames.
  uint32_t frames_checkerboarded_ = 0;
};

// Map of kCustom tracker results keyed by a sequence id.
using CustomTrackerResults =
    base::flat_map<int, FrameSequenceMetrics::ThroughputData>;

// Used for notifying attached FrameSequenceTracker's of begin-frames and
// submitted frames.
class CC_EXPORT FrameSequenceTrackerCollection {
 public:
  FrameSequenceTrackerCollection(
      bool is_single_threaded,
      CompositorFrameReportingController* frame_reporting_controller);
  ~FrameSequenceTrackerCollection();

  FrameSequenceTrackerCollection(const FrameSequenceTrackerCollection&) =
      delete;
  FrameSequenceTrackerCollection& operator=(
      const FrameSequenceTrackerCollection&) = delete;

  // Creates a tracker for the specified sequence-type.
  FrameSequenceMetrics* StartSequence(FrameSequenceTrackerType type);

  // Schedules |tracker| for destruction. This is preferred instead of outright
  // desrtruction of the tracker, since this ensures that the actual tracker
  // instance is destroyed *after* the presentation-feedbacks have been received
  // for all submitted frames.
  void StopSequence(FrameSequenceTrackerType type);

  // Creates a kCustom tracker for the given sequence id. It is an error and
  // DCHECKs if there is already a tracker associated with the sequence id.
  void StartCustomSequence(int sequence_id);

  // Schedules the kCustom tracker representing |sequence_id| for destruction.
  // It is a no-op if there is no tracker associated with the sequence id.
  // Similar to StopSequence above, the tracker instance is destroyed *after*
  // the presentation feedbacks have been received for all submitted frames.
  void StopCustomSequence(int sequence_id);

  // Removes all trackers. This also immediately destroys all trackers that had
  // been scheduled for destruction, even if there are pending
  // presentation-feedbacks. This is typically used if the client no longer
  // expects to receive presentation-feedbacks for the previously submitted
  // frames (e.g. when the gpu process dies).
  void ClearAll();

  // Notifies all trackers of various events.
  void NotifyBeginImplFrame(const viz::BeginFrameArgs& args);
  void NotifyBeginMainFrame(const viz::BeginFrameArgs& args);
  void NotifyMainFrameProcessed(const viz::BeginFrameArgs& args);
  void NotifyImplFrameCausedNoDamage(const viz::BeginFrameAck& ack);
  void NotifyMainFrameCausedNoDamage(const viz::BeginFrameArgs& args);
  void NotifyPauseFrameProduction();
  void NotifySubmitFrame(uint32_t frame_token,
                         bool has_missing_content,
                         const viz::BeginFrameAck& ack,
                         const viz::BeginFrameArgs& origin_args);
  void NotifyFrameEnd(const viz::BeginFrameArgs& args,
                      const viz::BeginFrameArgs& main_args);

  // Note that this notifies the trackers of the presentation-feedbacks, and
  // destroys any tracker that had been scheduled for destruction (using
  // |ScheduleRemoval()|) if it has no more pending frames. Data from non
  // kCustom typed trackers are reported to UMA. Data from kCustom typed
  // trackers are added to |custom_tracker_results_| for caller to pick up.
  void NotifyFramePresented(uint32_t frame_token,
                            const gfx::PresentationFeedback& feedback);

  // Return the type of each active frame tracker, encoded into a 16 bit
  // integer with the bit at each position corresponding to the enum value of
  // each type.
  ActiveFrameSequenceTrackers FrameSequenceTrackerActiveTypes();

  // Reports the accumulated kCustom tracker results and clears it.
  CustomTrackerResults TakeCustomTrackerResults();

  FrameSequenceTracker* GetTrackerForTesting(FrameSequenceTrackerType type);
  FrameSequenceTracker* GetRemovalTrackerForTesting(
      FrameSequenceTrackerType type);

  void SetUkmManager(UkmManager* manager);

 private:
  friend class FrameSequenceTrackerTest;

  void RecreateTrackers(const viz::BeginFrameArgs& args);
  // Destroy the trackers that are ready to be terminated.
  void DestroyTrackers();

  const bool is_single_threaded_;
  // The callsite can use the type to manipulate the tracker.
  base::flat_map<FrameSequenceTrackerType,
                 std::unique_ptr<FrameSequenceTracker>>
      frame_trackers_;

  // Custom trackers are keyed by a custom sequence id.
  base::flat_map<int, std::unique_ptr<FrameSequenceTracker>>
      custom_frame_trackers_;
  CustomTrackerResults custom_tracker_results_;

  std::vector<std::unique_ptr<FrameSequenceTracker>> removal_trackers_;
  CompositorFrameReportingController* const
      compositor_frame_reporting_controller_;

  // The reporter takes throughput data and connect to UkmManager to report it.
  std::unique_ptr<ThroughputUkmReporter> throughput_ukm_reporter_;

  base::flat_map<
      std::pair<FrameSequenceTrackerType, FrameSequenceMetrics::ThreadType>,
      std::unique_ptr<FrameSequenceMetrics>>
      accumulated_metrics_;
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

  static const char* GetFrameSequenceTrackerTypeName(
      FrameSequenceTrackerType type);

  ~FrameSequenceTracker();

  FrameSequenceTracker(const FrameSequenceTracker&) = delete;
  FrameSequenceTracker& operator=(const FrameSequenceTracker&) = delete;

  // Notifies the tracker when the compositor thread starts to process a
  // BeginFrameArgs.
  void ReportBeginImplFrame(const viz::BeginFrameArgs& args);

  // Notifies the tracker when a BeginFrameArgs is dispatched to the main
  // thread.
  void ReportBeginMainFrame(const viz::BeginFrameArgs& args);

  void ReportMainFrameProcessed(const viz::BeginFrameArgs& args);

  // Notifies the tracker when the compositor submits a CompositorFrame.
  // |origin_args| represents the BeginFrameArgs that triggered the update from
  // the main-thread.
  void ReportSubmitFrame(uint32_t frame_token,
                         bool has_missing_content,
                         const viz::BeginFrameAck& ack,
                         const viz::BeginFrameArgs& origin_args);

  void ReportFrameEnd(const viz::BeginFrameArgs& args,
                      const viz::BeginFrameArgs& main_args);

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

  // Returns true if we should ask this tracker to report its throughput data.
  bool ShouldReportMetricsNow(const viz::BeginFrameArgs& args) const;

  FrameSequenceMetrics* metrics() { return metrics_.get(); }
  FrameSequenceTrackerType type() const { return type_; }
  int custom_sequence_id() const { return custom_sequence_id_; }

  std::unique_ptr<FrameSequenceMetrics> TakeMetrics();

 private:
  friend class FrameSequenceTrackerCollection;
  friend class FrameSequenceTrackerTest;

  FrameSequenceTracker(FrameSequenceTrackerType type,
                       ThroughputUkmReporter* throughput_ukm_reporter,
                       int custom_sequence_id = -1);

  FrameSequenceMetrics::ThroughputData& impl_throughput() {
    return metrics_->impl_throughput();
  }
  FrameSequenceMetrics::ThroughputData& main_throughput() {
    return metrics_->main_throughput();
  }
  FrameSequenceMetrics::ThroughputData& aggregated_throughput() {
    return metrics_->aggregated_throughput();
  }

  void ScheduleTerminate();

  struct TrackedFrameData {
    // Represents the |BeginFrameArgs::source_id| and
    // |BeginFrameArgs::sequence_number| fields of the last processed
    // BeginFrameArgs.
    uint64_t previous_source = 0;
    uint64_t previous_sequence = 0;

    // The difference in |BeginFrameArgs::sequence_number| fields of the last
    // two processed BeginFrameArgs.
    uint32_t previous_sequence_delta = 0;
  };

  struct CheckerboardingData {
    CheckerboardingData();
    ~CheckerboardingData();

    // Tracks whether the last presented frame had checkerboarding. This is used
    // to track how many vsyncs showed frames with checkerboarding.
    bool last_frame_had_checkerboarding = false;

    base::TimeTicks last_frame_timestamp;

    // A list of frame-tokens that had checkerboarding.
    base::circular_deque<uint32_t> frames;
  };

  void UpdateTrackedFrameData(TrackedFrameData* frame_data,
                              uint64_t source_id,
                              uint64_t sequence_number);

  bool ShouldIgnoreBeginFrameSource(uint64_t source_id) const;

  bool ShouldIgnoreSequence(uint64_t sequence_number) const;

  const FrameSequenceTrackerType type_;
  const int custom_sequence_id_;

  TerminationStatus termination_status_ = TerminationStatus::kActive;

  TrackedFrameData begin_impl_frame_data_;
  TrackedFrameData begin_main_frame_data_;

  std::unique_ptr<FrameSequenceMetrics> metrics_;

  CheckerboardingData checkerboarding_;

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

  // Keeps track of the begin-main-frame that needs to be processed next.
  uint64_t awaiting_main_response_sequence_ = 0;

  // Keeps track of the last sequence-number that produced a frame from the
  // main-thread.
  uint64_t last_submitted_main_sequence_ = 0;

  // Keeps track of the last sequence-number that produced a frame that did not
  // have any damage from the main-thread.
  uint64_t last_no_main_damage_sequence_ = 0;

  // The time when this tracker is created, or the time when it was previously
  // scheduled to report histogram.
  base::TimeTicks first_frame_timestamp_;

  // Keeps track of whether the impl-frame being processed did not have any
  // damage from the compositor (i.e. 'impl damage').
  bool frame_had_no_compositor_damage_ = false;

  // Keeps track of whether a CompositorFrame is submitted during the frame.
  bool compositor_frame_submitted_ = false;
  bool submitted_frame_had_new_main_content_ = false;

  // Keeps track of whether the frame-states should be reset.
  bool reset_all_state_ = false;

  // A frame that is ignored at ReportSubmitFrame should never be presented.
  // TODO(xidachen): this should not be necessary. Some webview tests seem to
  // present a frame even if it is ignored by ReportSubmitFrame.
  base::flat_set<uint32_t> ignored_frame_tokens_;

  // Report the throughput metrics every 5 seconds.
  const base::TimeDelta time_delta_to_report_ = base::TimeDelta::FromSeconds(5);

  uint64_t last_started_impl_sequence_ = 0;
  uint64_t last_processed_impl_sequence_ = 0;

  uint64_t last_processed_main_sequence_ = 0;
  uint64_t last_processed_main_sequence_latency_ = 0;

  // Used to compute aggregated throughput.
  // When expecting a main frame, we accumulate the number of impl frames
  // presented because if that main frame ends up with no-damage, then we should
  // count the impl frames that were produced in the meantime.
  uint32_t impl_frames_produced_while_expecting_main_ = 0;
  // Each entry is a frame token, inserted at ReportSubmitFrame.
  base::circular_deque<uint32_t> expecting_main_when_submit_impl_;

  // Handle off-screen main damage case. In this case, the sequence is typically
  // like: b(1)B(0,1)E(1)n(1)e(1)b(2)n(2)e(2)...b(10)E(2)B(10,10)n(10)e(10).
  // Note that between two 'E's, all the impl frames caused no damage, and
  // no main frames were submitted or caused no damage.
  bool had_impl_frame_submitted_between_commits_ = false;
  uint64_t previous_begin_main_sequence_ = 0;
  // TODO(xidachen): remove this one.
  uint64_t current_begin_main_sequence_ = 0;

  // Tracks some data to generate useful trace events.
  struct TraceData {
    explicit TraceData(const void* trace_id);
    const void* trace_id;
    base::TimeTicks last_timestamp = base::TimeTicks::Now();
    int frame_count = 0;

    void Advance(base::TimeTicks new_timestamp);
  } trace_data_;

  // True when an impl-impl is not ended. A tracker is ready for termination
  // only when the last impl-frame is ended (ReportFrameEnd).
  bool is_inside_frame_ = false;

#if DCHECK_IS_ON()
  // This stringstream represents a sequence of frame reporting activities on
  // the current tracker. Each letter can be one of the following:
  // {'B', 'N', 'b', 'n', 'S', 'P'}, where
  // 'B' = ReportBeginMainFrame(), 'N' = ReportMainFrameCausedNoDamage(),
  // 'b' = ReportBeginImplFrame(), 'n' = ReportMainFrameCausedNoDamage(),
  // 'S' = ReportSubmitFrame() and 'P' = ReportFramePresented().
  // Note that |frame_sequence_trace_| is only defined and populated
  // when DCHECK is on.
  std::stringstream frame_sequence_trace_;

  // |frame_sequence_trace_| can be very long, in some cases we just need a
  // substring of it. This var tells us how many chars can be ignored from the
  // beginning of that debug string.
  unsigned ignored_trace_char_count_ = 0;

  // If ReportBeginImplFrame is never called on a arg, then ReportBeginMainFrame
  // should ignore that arg.
  base::flat_set<viz::BeginFrameId> impl_frames_;
#endif
};

}  // namespace cc

#endif  // CC_METRICS_FRAME_SEQUENCE_TRACKER_H_
