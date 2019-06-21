// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_COMPOSITOR_FRAME_REPORTER_H_
#define CC_SCHEDULER_COMPOSITOR_FRAME_REPORTER_H_

#include <vector>

#include "base/time/time.h"
#include "cc/base/base_export.h"
#include "cc/cc_export.h"

namespace cc {
class RollingTimeDeltaHistory;

// This is used for tracing and reporting the duration of pipeline stages within
// a single frame.
//
// For each stage in the frame pipeline, calling StartStage will start tracing
// that stage (and end any currently running stages).
//
// If the tracked frame is submitted (i.e. the frame termination status is
// kSubmittedFrame or kSubmittedFrameMissedDeadline), then the duration of each
// stage along with the total latency will be reported to UMA. These reported
// durations will be differentiated by whether the compositor is single threaded
// and whether the submitted frame missed the deadline. The format of each stage
// reported to UMA is "[SingleThreaded]Compositor.[MissedFrame.].<StageName>".
class CC_EXPORT CompositorFrameReporter {
 public:
  enum FrameTerminationStatus {
    // Compositor frame (with main thread updates) is submitted before a new
    // BeginImplFrame is issued (i.e. BF -> BMF -> Commit -> Activate ->
    // Submit).
    kSubmittedFrame,

    // Same as SubmittedFrame, but with the condition that there is another
    // frame being processed in the pipeline at an earlier stage.
    // This would imply that a new BeginImplFrame was issued during the lifetime
    // of this reporter, and therefore it missed its deadline
    // (e.g. BF1 -> BMF1 -> Submit -> BF2 -> Commit1 -> Activate1 -> BMF2 ->
    // Submit).
    kSubmittedFrameMissedDeadline,

    // Main frame was aborted; the reporter will not continue reporting.
    kMainFrameAborted,

    // Reporter that is currently at a stage is replaced by a new one (e.g. two
    // BeginImplFrames can happen without issuing BeginMainFrame, so the first
    // reporter would terminate with this status).
    // TODO(alsan): Track impl-only frames.
    kReplacedByNewReporter,

    // Frame that was being tracked did not end up being submitting (e.g. frame
    // had no damage or LTHI was ended).
    kDidNotProduceFrame,

    // Default termination status. Should not be reachable.
    kUnknown
  };

  enum class MissedFrameReportTypes {
    kNonMissedFrame,
    kMissedFrame,
    kMissedFrameLatencyIncrease,
    kMissedFrameReportTypeCount
  };

  enum class StageType {
    kBeginImplFrameToSendBeginMainFrame,
    kSendBeginMainFrameToCommit,
    kCommit,
    kEndCommitToActivation,
    kActivation,
    kEndActivateToSubmitCompositorFrame,
    kTotalLatency,
    kStageTypeCount
  };

  explicit CompositorFrameReporter(bool is_single_threaded = false);
  ~CompositorFrameReporter();

  CompositorFrameReporter(const CompositorFrameReporter& reporter) = delete;
  CompositorFrameReporter& operator=(const CompositorFrameReporter& reporter) =
      delete;

  // Note that the started stage may be reported to UMA. If the histogram is
  // intended to be reported then the histograms.xml file must be updated too.
  void StartStage(StageType stage_type,
                  base::TimeTicks start_time,
                  RollingTimeDeltaHistory* stage_time_delta_history);
  void TerminateFrame(FrameTerminationStatus termination_status,
                      base::TimeTicks termination_time);

  int StageHistorySizeForTesting() { return stage_history_.size(); }

 protected:
  struct StageData {
    StageType stage_type;
    base::TimeTicks start_time;
    base::TimeTicks end_time;
    RollingTimeDeltaHistory* time_delta_history;
  };

  StageData current_stage_;

  // Stage data is recorded here. On destruction these stages will be reported
  // to UMA if the termination status is kSubmittedFrame or
  // kSubmittedFrameMissedDeadline.
  std::vector<StageData> stage_history_;

 private:
  void TerminateReporter();
  void EndCurrentStage(base::TimeTicks end_time);
  void ReportStageHistograms(bool missed_frame) const;
  void ReportHistogram(
      CompositorFrameReporter::MissedFrameReportTypes report_type,
      StageType stage_type,
      base::TimeDelta time_delta) const;
  // Returns true if the stage duration is greater than |kAbnormalityPercentile|
  // of its RollingTimeDeltaHistory.
  base::TimeDelta GetStateNormalUpperLimit(const StageData& stage) const;

  const bool is_single_threaded_;
  base::TimeTicks frame_termination_time_;
  FrameTerminationStatus frame_termination_status_ =
      FrameTerminationStatus::kUnknown;
};
}  // namespace cc

#endif  // CC_SCHEDULER_COMPOSITOR_FRAME_REPORTER_H_"
