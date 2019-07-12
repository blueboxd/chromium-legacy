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
  enum class FrameTerminationStatus {
    // The tracked compositor frame was presented.
    kPresentedFrame,

    // The tracked compositor frame was submitted to the display compositor but
    // was not presented.
    kDidNotPresentFrame,

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
    kSubmitCompositorFrameToPresentationCompositorFrame,
    kTotalLatency,
    kStageTypeCount
  };

  explicit CompositorFrameReporter(bool is_single_threaded = false);
  ~CompositorFrameReporter();

  CompositorFrameReporter(const CompositorFrameReporter& reporter) = delete;
  CompositorFrameReporter& operator=(const CompositorFrameReporter& reporter) =
      delete;

  void MissedSubmittedFrame();

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
  // to UMA if the termination status is |kPresentedFrame|. Reported data will
  // be divided based on the frame submission status.
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
  bool submitted_frame_missed_deadline_ = false;
  base::TimeTicks frame_termination_time_;
  FrameTerminationStatus frame_termination_status_ =
      FrameTerminationStatus::kUnknown;
};
}  // namespace cc

#endif  // CC_SCHEDULER_COMPOSITOR_FRAME_REPORTER_H_"
